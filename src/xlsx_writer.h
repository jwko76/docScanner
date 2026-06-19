// xlsx_writer.h  –  Zero-dependency XLSX file writer for PiiScanner
//
// Implementation:
//   - ZIP STORED format (no compression, no zlib)
//   - CRC32 via 256-entry lookup table
//   - xlsx XML generated with std::string concatenation
//   - Header-only, C++17, no external includes beyond STL
//
// Predefined format IDs (match reporter.cpp usage):
//   XLFMT_DEFAULT  0  – no style
//   XLFMT_HEADER   1  – bold white text on blue (#2F5496), centered, border
//   XLFMT_CELL     2  – thin border, wrap text
//   XLFMT_CELL_RED 3  – thin border, bold red text (#C00000)
//   XLFMT_NUM      4  – thin border, right-aligned (numbers)
//   XLFMT_TITLE    5  – bold 14pt (section titles)

#pragma once
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <fstream>
#include <algorithm>
#include <cstdio>   // snprintf

#define XLFMT_DEFAULT  0
#define XLFMT_HEADER   1
#define XLFMT_CELL     2
#define XLFMT_CELL_RED 3
#define XLFMT_NUM      4
#define XLFMT_TITLE    5

// ============================================================
// XlsxWriter
// ============================================================
class XlsxWriter {
public:
    // Add a worksheet; returns 0-based sheet index
    int addWorksheet(const std::string& name) {
        Sheet sh;
        sh.name = name;
        m_sheets.push_back(std::move(sh));
        return (int)m_sheets.size() - 1;
    }

    // Write a UTF-8 string to (row, col) [0-based]
    void writeString(int si, int row, int col,
                     const std::string& utf8, int fmt = XLFMT_CELL) {
        if (si < 0 || si >= (int)m_sheets.size()) return;
        Sheet& sh = m_sheets[si];
        int idx = addStr(utf8);
        Cell c; c.isStr = true; c.strIdx = idx; c.num = 0.0; c.fmt = fmt;
        sh.cells[{row, col}] = c;
        if (row > sh.maxRow) sh.maxRow = row;
        if (col > sh.maxCol) sh.maxCol = col;
    }

    // Write a number to (row, col) [0-based]
    void writeNumber(int si, int row, int col,
                     double num, int fmt = XLFMT_CELL) {
        if (si < 0 || si >= (int)m_sheets.size()) return;
        Sheet& sh = m_sheets[si];
        Cell c; c.isStr = false; c.strIdx = 0; c.num = num; c.fmt = fmt;
        sh.cells[{row, col}] = c;
        if (row > sh.maxRow) sh.maxRow = row;
        if (col > sh.maxCol) sh.maxCol = col;
    }

    // Set column width (0-based col)
    void setColumnWidth(int si, int col, double width) {
        if (si < 0 || si >= (int)m_sheets.size()) return;
        m_sheets[si].colWidths[col] = width;
    }

    // Enable auto-filter on a cell range [0-based, inclusive]
    void setAutoFilter(int si, int r1, int c1, int r2, int c2) {
        if (si < 0 || si >= (int)m_sheets.size()) return;
        Sheet& sh = m_sheets[si];
        sh.hasFilter = true;
        sh.fr = r1; sh.fc = c1; sh.lr = r2; sh.lc = c2;
    }

    // Write xlsx file to disk; returns true on success
    bool save(const std::string& filepath) {
        using Entry = std::pair<std::string, std::string>;
        std::vector<Entry> entries;
        entries.push_back({"[Content_Types].xml",       genContentTypes()});
        entries.push_back({"_rels/.rels",               genRels()});
        entries.push_back({"xl/workbook.xml",           genWorkbook()});
        entries.push_back({"xl/_rels/workbook.xml.rels",genWorkbookRels()});
        entries.push_back({"xl/styles.xml",             genStyles()});
        entries.push_back({"xl/sharedStrings.xml",      genSharedStrings()});
        for (int i = 0; i < (int)m_sheets.size(); i++) {
            entries.push_back({
                "xl/worksheets/sheet" + std::to_string(i+1) + ".xml",
                genSheet(m_sheets[i])
            });
        }
        return writeZip(filepath, entries);
    }

private:
    // ----------------------------------------------------------
    // Data structures
    // ----------------------------------------------------------
    struct Cell {
        bool   isStr  = false;
        int    strIdx = 0;
        double num    = 0.0;
        int    fmt    = 0;
    };

    struct Sheet {
        std::string name;
        std::map<std::pair<int,int>, Cell> cells;
        std::map<int, double> colWidths;
        int  maxRow    = 0;
        int  maxCol    = 0;
        bool hasFilter = false;
        int  fr = 0, fc = 0, lr = 0, lc = 0;
    };

    std::vector<Sheet>        m_sheets;
    std::vector<std::string>  m_strings;
    std::map<std::string,int> m_strMap;

    // ----------------------------------------------------------
    // Shared strings
    // ----------------------------------------------------------
    int addStr(const std::string& s) {
        auto it = m_strMap.find(s);
        if (it != m_strMap.end()) return it->second;
        int idx = (int)m_strings.size();
        m_strings.push_back(s);
        m_strMap[s] = idx;
        return idx;
    }

    // ----------------------------------------------------------
    // XML helpers
    // ----------------------------------------------------------
    static std::string xe(const std::string& s) {
        std::string r;
        r.reserve(s.size() + 16);
        for (unsigned char c : s) {
            if      (c == '&')  r += "&amp;";
            else if (c == '<')  r += "&lt;";
            else if (c == '>')  r += "&gt;";
            else if (c == '"')  r += "&quot;";
            else if (c < 0x20 && c != 0x09 && c != 0x0A && c != 0x0D)
                /* skip illegal XML control chars */ ;
            else r += (char)c;
        }
        return r;
    }

    // 0-based column index → Excel column name ("A", "B", ..., "Z", "AA", ...)
    static std::string colRef(int c) {
        std::string r;
        int n = c + 1;
        while (n > 0) {
            r = (char)('A' + (n - 1) % 26) + r;
            n = (n - 1) / 26;
        }
        return r;
    }

    // 0-based (row, col) → cell reference "A1", "B3", etc.
    static std::string cellRef(int row, int col) {
        return colRef(col) + std::to_string(row + 1);
    }

    // double → XML-safe string (uses "C" locale decimal point)
    static std::string numStr(double v) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.10g", v);
        return buf;
    }

    // ----------------------------------------------------------
    // XML generators
    // ----------------------------------------------------------
    std::string genContentTypes() const {
        std::string o =
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
            "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
            "<Default Extension=\"rels\""
            " ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
            "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
            "<Override PartName=\"/xl/workbook.xml\""
            " ContentType=\"application/vnd.openxmlformats-officedocument"
            ".spreadsheetml.sheet.main+xml\"/>"
            "<Override PartName=\"/xl/styles.xml\""
            " ContentType=\"application/vnd.openxmlformats-officedocument"
            ".spreadsheetml.styles+xml\"/>"
            "<Override PartName=\"/xl/sharedStrings.xml\""
            " ContentType=\"application/vnd.openxmlformats-officedocument"
            ".spreadsheetml.sharedStrings+xml\"/>";
        for (int i = 0; i < (int)m_sheets.size(); i++) {
            o += "<Override PartName=\"/xl/worksheets/sheet"
                 + std::to_string(i+1) + ".xml\""
                 " ContentType=\"application/vnd.openxmlformats-officedocument"
                 ".spreadsheetml.worksheet+xml\"/>";
        }
        o += "</Types>";
        return o;
    }

    static std::string genRels() {
        return
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
            "<Relationships xmlns=\"http://schemas.openxmlformats.org"
            "/package/2006/relationships\">"
            "<Relationship Id=\"rId1\""
            " Type=\"http://schemas.openxmlformats.org/officeDocument"
            "/2006/relationships/officeDocument\""
            " Target=\"xl/workbook.xml\"/>"
            "</Relationships>";
    }

    std::string genWorkbook() const {
        std::string o =
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
            "<workbook xmlns=\"http://schemas.openxmlformats.org"
            "/spreadsheetml/2006/main\""
            " xmlns:r=\"http://schemas.openxmlformats.org/officeDocument"
            "/2006/relationships\">"
            "<sheets>";
        for (int i = 0; i < (int)m_sheets.size(); i++) {
            o += "<sheet name=\"" + xe(m_sheets[i].name) + "\""
                 " sheetId=\"" + std::to_string(i+1) + "\""
                 " r:id=\"rId" + std::to_string(i+1) + "\"/>";
        }
        o += "</sheets></workbook>";
        return o;
    }

    std::string genWorkbookRels() const {
        std::string o =
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
            "<Relationships xmlns=\"http://schemas.openxmlformats.org"
            "/package/2006/relationships\">";
        for (int i = 0; i < (int)m_sheets.size(); i++) {
            o += "<Relationship Id=\"rId" + std::to_string(i+1) + "\""
                 " Type=\"http://schemas.openxmlformats.org/officeDocument"
                 "/2006/relationships/worksheet\""
                 " Target=\"worksheets/sheet" + std::to_string(i+1) + ".xml\"/>";
        }
        int n = (int)m_sheets.size();
        o += "<Relationship Id=\"rId" + std::to_string(n+1) + "\""
             " Type=\"http://schemas.openxmlformats.org/officeDocument"
             "/2006/relationships/styles\""
             " Target=\"styles.xml\"/>"
             "<Relationship Id=\"rId" + std::to_string(n+2) + "\""
             " Type=\"http://schemas.openxmlformats.org/officeDocument"
             "/2006/relationships/sharedStrings\""
             " Target=\"sharedStrings.xml\"/>"
             "</Relationships>";
        return o;
    }

    // Styles: 4 fonts / 3 fills / 2 borders / 6 cell formats
    static std::string genStyles() {
        return
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
            "<styleSheet xmlns=\"http://schemas.openxmlformats.org"
            "/spreadsheetml/2006/main\">"

            // ---- fonts (idx 0-3) ----
            "<fonts count=\"4\">"
            // 0: default 11pt
            "<font><sz val=\"11\"/><name val=\"\xEB\xA7\x91\xEC\x9D\x80 \xEA\xB3\xA0\xEB\x94\xB1\"/></font>"
            // 1: bold + white  (HEADER)
            "<font><b/><sz val=\"11\"/><color rgb=\"FFFFFFFF\"/>"
            "<name val=\"\xEB\xA7\x91\xEC\x9D\x80 \xEA\xB3\xA0\xEB\x94\xB1\"/></font>"
            // 2: bold + red C00000  (CELL_RED)
            "<font><b/><sz val=\"11\"/><color rgb=\"FFC00000\"/>"
            "<name val=\"\xEB\xA7\x91\xEC\x9D\x80 \xEA\xB3\xA0\xEB\x94\xB1\"/></font>"
            // 3: bold 14pt  (TITLE)
            "<font><b/><sz val=\"14\"/>"
            "<name val=\"\xEB\xA7\x91\xEC\x9D\x80 \xEA\xB3\xA0\xEB\x94\xB1\"/></font>"
            "</fonts>"

            // ---- fills (idx 0-2; 0+1 are required) ----
            "<fills count=\"3\">"
            "<fill><patternFill patternType=\"none\"/></fill>"
            "<fill><patternFill patternType=\"gray125\"/></fill>"
            // 2: solid blue 2F5496  (HEADER background)
            "<fill><patternFill patternType=\"solid\">"
            "<fgColor rgb=\"FF2F5496\"/><bgColor indexed=\"64\"/>"
            "</patternFill></fill>"
            "</fills>"

            // ---- borders (idx 0-1) ----
            "<borders count=\"2\">"
            // 0: none
            "<border><left/><right/><top/><bottom/><diagonal/></border>"
            // 1: thin all sides
            "<border>"
            "<left style=\"thin\"><color auto=\"1\"/></left>"
            "<right style=\"thin\"><color auto=\"1\"/></right>"
            "<top style=\"thin\"><color auto=\"1\"/></top>"
            "<bottom style=\"thin\"><color auto=\"1\"/></bottom>"
            "<diagonal/>"
            "</border>"
            "</borders>"

            // ---- base cell style (required) ----
            "<cellStyleXfs count=\"1\">"
            "<xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\"/>"
            "</cellStyleXfs>"

            // ---- cell formats (idx 0-5 = XLFMT_*) ----
            "<cellXfs count=\"6\">"
            // 0: DEFAULT
            "<xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\"/>"
            // 1: HEADER – blue bg, white bold, centered, wrapped, border
            "<xf numFmtId=\"0\" fontId=\"1\" fillId=\"2\" borderId=\"1\" xfId=\"0\""
            " applyFont=\"1\" applyFill=\"1\" applyBorder=\"1\" applyAlignment=\"1\">"
            "<alignment horizontal=\"center\" wrapText=\"1\"/></xf>"
            // 2: CELL – border, wrapped
            "<xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"1\" xfId=\"0\""
            " applyBorder=\"1\" applyAlignment=\"1\">"
            "<alignment wrapText=\"1\"/></xf>"
            // 3: CELL_RED – border, bold red
            "<xf numFmtId=\"0\" fontId=\"2\" fillId=\"0\" borderId=\"1\" xfId=\"0\""
            " applyFont=\"1\" applyBorder=\"1\"/>"
            // 4: NUM – border, right-aligned
            "<xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"1\" xfId=\"0\""
            " applyBorder=\"1\" applyAlignment=\"1\">"
            "<alignment horizontal=\"right\"/></xf>"
            // 5: TITLE – bold 14pt, no border
            "<xf numFmtId=\"0\" fontId=\"3\" fillId=\"0\" borderId=\"0\" xfId=\"0\""
            " applyFont=\"1\"/>"
            "</cellXfs>"

            "<cellStyles count=\"1\">"
            "<cellStyle name=\"Normal\" xfId=\"0\" builtinId=\"0\"/>"
            "</cellStyles>"
            "</styleSheet>";
    }

    std::string genSharedStrings() const {
        std::string o =
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
            "<sst xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\""
            " count=\"" + std::to_string(m_strings.size()) +
            "\" uniqueCount=\"" + std::to_string(m_strings.size()) + "\">";
        for (const auto& s : m_strings)
            o += "<si><t xml:space=\"preserve\">" + xe(s) + "</t></si>";
        o += "</sst>";
        return o;
    }

    std::string genSheet(const Sheet& sh) const {
        std::string o =
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
            "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\""
            " xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
            "<sheetViews><sheetView workbookViewId=\"0\">"
            "<selection activeCell=\"A1\"/></sheetView></sheetViews>";

        // Column widths
        if (!sh.colWidths.empty()) {
            o += "<cols>";
            for (const auto& kv : sh.colWidths) {
                char buf[128];
                snprintf(buf, sizeof(buf),
                    "<col min=\"%d\" max=\"%d\" width=\"%.2f\" customWidth=\"1\"/>",
                    kv.first + 1, kv.first + 1, kv.second);
                o += buf;
            }
            o += "</cols>";
        }

        // Sheet data
        o += "<sheetData>";

        // Group cells by row
        std::map<int, std::vector<std::pair<int, const Cell*>>> byRow;
        for (const auto& kv : sh.cells)
            byRow[kv.first.first].push_back({kv.first.second, &kv.second});

        for (auto& rowEntry : byRow) {
            int row = rowEntry.first;
            auto& cols = rowEntry.second;
            std::sort(cols.begin(), cols.end(),
                [](const auto& a, const auto& b) { return a.first < b.first; });

            o += "<row r=\"" + std::to_string(row + 1) + "\">";
            for (const auto& colEntry : cols) {
                int col           = colEntry.first;
                const Cell& cell  = *colEntry.second;
                std::string ref   = cellRef(row, col);

                if (cell.isStr) {
                    o += "<c r=\"" + ref + "\" t=\"s\"";
                    if (cell.fmt) o += " s=\"" + std::to_string(cell.fmt) + "\"";
                    o += "><v>" + std::to_string(cell.strIdx) + "</v></c>";
                } else {
                    o += "<c r=\"" + ref + "\"";
                    if (cell.fmt) o += " s=\"" + std::to_string(cell.fmt) + "\"";
                    o += "><v>" + numStr(cell.num) + "</v></c>";
                }
            }
            o += "</row>";
        }
        o += "</sheetData>";

        if (sh.hasFilter) {
            o += "<autoFilter ref=\""
                 + cellRef(sh.fr, sh.fc) + ":" + cellRef(sh.lr, sh.lc)
                 + "\"/>";
        }

        o += "</worksheet>";
        return o;
    }

    // ----------------------------------------------------------
    // CRC32  (IEEE 802.3 / ZIP polynomial 0xEDB88320)
    // ----------------------------------------------------------
    static uint32_t crc32(const uint8_t* data, size_t len) {
        static const uint32_t T[256] = {
            0x00000000,0x77073096,0xEE0E612C,0x990951BA,0x076DC419,0x706AF48F,
            0xE963A535,0x9E6495A3,0x0EDB8832,0x79DCB8A4,0xE0D5E91B,0x97D2D988,
            0x09B64C2B,0x7EB17CBF,0xE7B82D09,0x90BF1D3D,0x1DB71064,0x6AB020F2,
            0xF3B97148,0x84BE41DE,0x1ADAD47D,0x6DDDE4EB,0xF4D4B551,0x83D385C7,
            0x136C9856,0x646BA8C0,0xFD62F97A,0x8A65C9EC,0x14015C4F,0x63066CD9,
            0xFA0F3D63,0x8D080DF5,0x3B6E20C8,0x4C69105E,0xD56041E4,0xA2677172,
            0x3C03E4D1,0x4B04D447,0xD20D85FD,0xA50AB56B,0x35B5A8FA,0x42B2986C,
            0xDBBBC9D6,0xACBCB040,0x32D86CE3,0x45DF5C75,0xDCD60DCF,0xABD13D59,
            0x26D930AC,0x51DE003A,0xC8D75180,0xBFD06116,0x21B4F928,0x56B3C423,
            0xCFBA9599,0xB8BDA50F,0x2802B89E,0x5F058808,0xC60CD9B2,0xB10BE924,
            0x2F6F7C87,0x58684C11,0xC1611DAB,0xB6662D3D,0x76DC4190,0x01DB7106,
            0x98D220BC,0xEFD5102A,0x71B18589,0x06B6B51F,0x9FBFE4A5,0xE8B8D433,
            0x7807C9A2,0x0F00F934,0x9609A88E,0xE10E9818,0x7F6A0DBB,0x086D3D2D,
            0x91646C97,0xE6635C01,0x6B6B51F4,0x1C6C6162,0x856530D8,0xF262004E,
            0x6C0695ED,0x1B01A57B,0x8208F4C1,0xF50FC457,0x65B0D9C6,0x12B7E950,
            0x8BBEB8EA,0xFCB9887C,0x62DD1DDF,0x15DA2D49,0x8CD37CF3,0xFBD44C65,
            0x4DB26158,0x3AB551CE,0xA3BC0074,0xD4BB30E2,0x4ADFA541,0x3DD895D7,
            0xA4D1C46D,0xD3D6F4FB,0x4369E96A,0x346ED9FC,0xAD678846,0xDA60B8D0,
            0x44042D73,0x33031DE5,0xAA0A4C5F,0xDD0D7CC9,0x5005713C,0x270241AA,
            0xBE0B1010,0xC90C2086,0x5768B525,0x206F85B3,0xB966D409,0xCE61E49F,
            0x5EDEF90E,0x29D9C998,0xB0D09822,0xC7D7A8B4,0x59B33D17,0x2EB40D81,
            0xB7BD5C3B,0xC0BA6CAD,0xEDB88320,0x9ABFB3B6,0x03B6E20C,0x74B1D29A,
            0xEAD54739,0x9DD277AF,0x04DB2615,0x73DC1683,0xE3630B12,0x94643B84,
            0x0D6D6A3E,0x7A6A5AA8,0xE40ECF0B,0x9309FF9D,0x0A00AE27,0x7D079EB1,
            0xF00F9344,0x8708A3D2,0x1E01F268,0x6906C2FE,0xF762575D,0x806567CB,
            0x196C3671,0x6E6B06E7,0xFED41B76,0x89D32BE0,0x10DA7A5A,0x67DD4ACC,
            0xF9B9DF6F,0x8EBEEFF9,0x17B7BE43,0x60B08ED5,0xD6D6A3E8,0xA1D1937E,
            0x38D8C2C4,0x4FDFF252,0xD1BB67F1,0xA6BC5767,0x3FB506DD,0x48B2364B,
            0xD80D2BDA,0xAF0A1B4C,0x36034AF6,0x41047A60,0xDF60EFC3,0xA867DF55,
            0x316658EF,0x4669BE79,0xCB61B38C,0xBC66831A,0x256FD2A0,0x5268E236,
            0xCC0C7795,0xBB0B4703,0x220216B9,0x5505262F,0xC5BA3BBE,0xB2BD0B28,
            0x2BB45A92,0x5CB36A04,0xC2D7FFA7,0xB5D0CF31,0x2CD99E8B,0x5BDEAE1D,
            0x9B64C2B0,0xEC63F226,0x756AA39C,0x026D930A,0x9C0906A9,0xEB0E363F,
            0x72076785,0x05005713,0x95BF4A82,0xE2B87A14,0x7BB12BAE,0x0CB61B38,
            0x92D28E9B,0xE5D5BE0D,0x7CDCEFB7,0x0BDBDF21,0x86D3D2D4,0xF1D4E242,
            0x68DDB3F8,0x1FDA836E,0x81BE16CD,0xF6B9265B,0x6FB077E1,0x18B74777,
            0x88085AE6,0xFF0F6A70,0x66063BCA,0x11010B5C,0x8F659EFF,0xF862AE69,
            0x616BFFD3,0x166CCF45,0xA00AE278,0xD70DD2EE,0x4E048354,0x3903B3C2,
            0xA7672661,0xD06016F7,0x4969474D,0x3E6E77DB,0xAED16A4A,0xD9D65ADC,
            0x40DF0B66,0x37D83BF0,0xA9BCAE53,0xDEBB9EC5,0x47B2CF7F,0x30B5FFE9,
            0xBDBDF21C,0xCABAC28A,0x53B39330,0x24B4A3A6,0xBAD03605,0xCDD70693,
            0x54DE5729,0x23D967BF,0xB3667A2E,0xC4614AB8,0x5D681B02,0x2A6F2B94,
            0xB40BBE37,0xC30C8EA1,0x5A05DF1B,0x2D02EF8D
        };
        uint32_t crc = 0xFFFFFFFF;
        for (size_t i = 0; i < len; i++)
            crc = (crc >> 8) ^ T[(crc ^ data[i]) & 0xFF];
        return crc ^ 0xFFFFFFFF;
    }

    // ----------------------------------------------------------
    // ZIP STORED writer  (RFC 1951 / PKZIP format)
    // ----------------------------------------------------------
    static void u16le(std::vector<uint8_t>& b, uint16_t v) {
        b.push_back( v        & 0xFF);
        b.push_back((v >>  8) & 0xFF);
    }
    static void u32le(std::vector<uint8_t>& b, uint32_t v) {
        b.push_back( v        & 0xFF);
        b.push_back((v >>  8) & 0xFF);
        b.push_back((v >> 16) & 0xFF);
        b.push_back((v >> 24) & 0xFF);
    }
    static void appendBytes(std::vector<uint8_t>& b, const void* p, size_t n) {
        const uint8_t* q = static_cast<const uint8_t*>(p);
        b.insert(b.end(), q, q + n);
    }

    static bool writeZip(const std::string& path,
                         const std::vector<std::pair<std::string,std::string>>& entries) {
        std::vector<uint8_t> buf;
        buf.reserve(1 << 20); // 1 MB initial

        struct CdEntry { std::string name; uint32_t crc, size, offset; };
        std::vector<CdEntry> cd;

        for (const auto& e : entries) {
            const std::string& name = e.first;
            const std::string& data = e.second;

            uint32_t off = static_cast<uint32_t>(buf.size());
            uint32_t sz  = static_cast<uint32_t>(data.size());
            uint32_t crc = crc32(reinterpret_cast<const uint8_t*>(data.data()), sz);

            // Local file header  (30 bytes + filename)
            appendBytes(buf, "PK\x03\x04", 4);
            u16le(buf, 20);    // version needed: 2.0
            u16le(buf, 0);     // general purpose bit flag
            u16le(buf, 0);     // compression method: STORED
            u16le(buf, 0);     // last mod time
            u16le(buf, 0);     // last mod date
            u32le(buf, crc);
            u32le(buf, sz);    // compressed size = uncompressed size
            u32le(buf, sz);
            u16le(buf, static_cast<uint16_t>(name.size()));
            u16le(buf, 0);     // extra field length
            appendBytes(buf, name.data(), name.size());
            appendBytes(buf, data.data(), sz);

            CdEntry cde; cde.name = name; cde.crc = crc; cde.size = sz; cde.offset = off;
            cd.push_back(cde);
        }

        // Central directory
        uint32_t cdOffset = static_cast<uint32_t>(buf.size());
        for (const auto& e : cd) {
            appendBytes(buf, "PK\x01\x02", 4);
            u16le(buf, 0x0314); // version made by: 3.0, Unix
            u16le(buf, 20);     // version needed: 2.0
            u16le(buf, 0);      // flags
            u16le(buf, 0);      // compression: STORED
            u16le(buf, 0);      // mod time
            u16le(buf, 0);      // mod date
            u32le(buf, e.crc);
            u32le(buf, e.size);
            u32le(buf, e.size);
            u16le(buf, static_cast<uint16_t>(e.name.size()));
            u16le(buf, 0);      // extra field len
            u16le(buf, 0);      // comment len
            u16le(buf, 0);      // disk number start
            u16le(buf, 0);      // internal attributes
            u32le(buf, 0);      // external attributes
            u32le(buf, e.offset);
            appendBytes(buf, e.name.data(), e.name.size());
        }
        uint32_t cdSize = static_cast<uint32_t>(buf.size()) - cdOffset;

        // End of central directory record  (22 bytes)
        appendBytes(buf, "PK\x05\x06", 4);
        u16le(buf, 0);                                       // disk number
        u16le(buf, 0);                                       // disk with CD
        u16le(buf, static_cast<uint16_t>(cd.size()));        // entries this disk
        u16le(buf, static_cast<uint16_t>(cd.size()));        // total entries
        u32le(buf, cdSize);                                  // CD size
        u32le(buf, cdOffset);                               // CD offset
        u16le(buf, 0);                                       // comment length

        // Write to file
        std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
        if (!ofs) return false;
        ofs.write(reinterpret_cast<const char*>(buf.data()),
                  static_cast<std::streamsize>(buf.size()));
        return ofs.good();
    }
};
