// text_extractor.cpp
// 문서/이미지에서 텍스트를 추출하는 모듈
//
// 추출 전략:
//   - .txt/.csv/.log 등: 직접 파일 읽기 (인코딩 자동 감지)
//   - .docx/.xlsx/.pptx: (1) Windows IFilter → (2) OOXML XML 직접 파싱
//   - .doc/.xls/.ppt/.pdf/.hwp/.hwpx: Windows IFilter
//   - 이미지: Windows OCR API (WinRT, Windows 10+)

#include "text_extractor.h"

#include <windows.h>
#include <filter.h>       // IFilter, STAT_CHUNK
#include <filterr.h>      // FILTER_E_*
// LoadIFilter is in query.lib; forward-declare since its header location varies by SDK version
extern "C" HRESULT __stdcall LoadIFilter(LPCWSTR pwcsPath, IUnknown* pUnkOuter, void** ppIUnk);
#include <shlwapi.h>
#include <objbase.h>

// WinRT / Windows OCR
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Ocr.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Globalization.h>
#include <winrt/Windows.Foundation.Collections.h>  // begin()/end() for IVectorView range-for

#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <stdexcept>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "query.lib")

using namespace winrt;
using namespace winrt::Windows::Media::Ocr;
using namespace winrt::Windows::Graphics::Imaging;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Streams;
using namespace winrt::Windows::Globalization;

// ============================================================
// 생성자 / 소멸자
// ============================================================

TextExtractor::TextExtractor() {
    // COM 초기화 (멀티스레드 아파트)
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    // WinRT 초기화
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
}

TextExtractor::~TextExtractor() {
    CoUninitialize();
}

// ============================================================
// 공개 인터페이스: extract
// ============================================================

ExtractionResult TextExtractor::extract(const std::wstring& filePath,
                                         const std::wstring& extension) {
    // 이미지
    const auto& imgExts = { L".jpg", L".jpeg", L".png",
                             L".tif", L".tiff", L".bmp", L".gif", L".webp" };
    for (const auto& e : imgExts) {
        if (extension == e) return extractViaWinOcr(filePath);
    }

    // 일반 텍스트
    const auto& txtExts = { L".txt", L".log", L".csv", L".tsv",
                             L".xml", L".json", L".html", L".htm",
                             L".ini", L".cfg", L".conf" };
    for (const auto& e : txtExts) {
        if (extension == e) return extractPlainText(filePath);
    }

    // OOXML (IFilter → fallback 직접 파싱)
    const auto& ooxmlExts = { L".docx", L".xlsx", L".pptx" };
    for (const auto& e : ooxmlExts) {
        if (extension == e) {
            auto r = extractViaIFilter(filePath);
            if (!r.success || r.text.empty()) {
                // Fallback: ZIP+XML 직접 파싱
                return extractOoxml(filePath, extension);
            }
            return r;
        }
    }

    // 나머지 (doc, xls, ppt, pdf, hwp, hwpx, rtf 등) → IFilter
    return extractViaIFilter(filePath);
}

// ============================================================
// 일반 텍스트 파일 읽기
// ============================================================

ExtractionResult TextExtractor::extractPlainText(const std::wstring& filePath) {
    ExtractionResult res;
    res.method = L"PlainText";

    auto bytes = readFileBytes(filePath, m_maxTextLength * 4); // 바이트 과다 읽기
    if (bytes.empty()) {
        res.errorMessage = L"파일 읽기 실패 또는 빈 파일";
        return res;
    }

    res.text = toWString(bytes);
    if (res.text.size() > m_maxTextLength)
        res.text.resize(m_maxTextLength);

    res.success = true;
    return res;
}

// ============================================================
// Windows IFilter 기반 텍스트 추출
// ============================================================

ExtractionResult TextExtractor::extractViaIFilter(const std::wstring& filePath) {
    ExtractionResult res;
    res.method = L"IFilter";

    IFilter* pFilter = nullptr;
    HRESULT hr = LoadIFilter(filePath.c_str(), nullptr, (LPVOID*)&pFilter);
    if (FAILED(hr) || !pFilter) {
        res.errorMessage = L"IFilter 로드 실패 (HRESULT=" +
                           std::to_wstring((unsigned long)hr) + L")";
        return res;
    }

    // IFilter 초기화
    ULONG dwFlags = 0;
    hr = pFilter->Init(IFILTER_INIT_APPLY_INDEX_ATTRIBUTES |
                       IFILTER_INIT_SEARCH_LINKS, 0, nullptr, &dwFlags);
    if (FAILED(hr)) {
        pFilter->Release();
        res.errorMessage = L"IFilter::Init 실패";
        return res;
    }

    std::wstring text;
    text.reserve(65536);

    STAT_CHUNK chunk;
    while (true) {
        hr = pFilter->GetChunk(&chunk);
        if (hr == FILTER_E_END_OF_CHUNKS || FAILED(hr)) break;

        if (chunk.flags & CHUNK_TEXT) {
            wchar_t buf[8192];
            while (true) {
                ULONG cchText = (ULONG)std::size(buf);
                hr = pFilter->GetText(&cchText, buf);
                if (hr == FILTER_E_NO_MORE_TEXT || FAILED(hr)) break;
                if (cchText > 0) {
                    text.append(buf, cchText);
                }
                if (text.size() >= m_maxTextLength) break;
            }
        }
        if (text.size() >= m_maxTextLength) break;
    }

    pFilter->Release();

    if (text.empty()) {
        res.errorMessage = L"IFilter가 텍스트를 추출하지 못함";
        return res;
    }

    res.text    = std::move(text);
    res.success = true;
    return res;
}

// ============================================================
// OOXML (docx/xlsx/pptx) 직접 파싱 - IFilter fallback
// ZIP 내 XML을 파싱하여 텍스트 추출
// ============================================================

// 간단한 ZIP 구조체 (로컬 파일 헤더)
struct ZipLocalHeader {
    uint32_t signature;       // 0x04034b50
    uint16_t versionNeeded;
    uint16_t flags;
    uint16_t compression;
    uint16_t modTime;
    uint16_t modDate;
    uint32_t crc32;
    uint32_t compressedSize;
    uint32_t uncompressedSize;
    uint16_t fileNameLength;
    uint16_t extraFieldLength;
};

// 간단한 XML 텍스트 태그 추출 함수
static void extractTagText(const std::wstring& xml,
                            const std::wstring& tag,
                            std::wstring& out) {
    std::wstring openTag  = L"<" + tag + L">";
    std::wstring openTagAttr = L"<" + tag + L" ";  // 속성이 있는 경우
    std::wstring closeTag = L"</" + tag + L">";

    size_t pos = 0;
    while (pos < xml.size()) {
        size_t start = xml.find(openTag, pos);
        size_t startAttr = xml.find(openTagAttr, pos);
        if (start == std::wstring::npos && startAttr == std::wstring::npos) break;

        // 더 앞에 있는 것 선택
        if (start == std::wstring::npos) start = startAttr;
        else if (startAttr != std::wstring::npos) start = std::min(start, startAttr);

        // 태그 닫는 '>' 찾기
        size_t tagEnd = xml.find(L'>', start);
        if (tagEnd == std::wstring::npos) break;

        size_t end = xml.find(closeTag, tagEnd);
        if (end == std::wstring::npos) { pos = tagEnd + 1; continue; }

        std::wstring content = xml.substr(tagEnd + 1, end - tagEnd - 1);
        // 내부 태그 제거 (간단 strip)
        std::wstring stripped;
        bool inTag = false;
        for (wchar_t c : content) {
            if (c == L'<') inTag = true;
            else if (c == L'>') inTag = false;
            else if (!inTag) stripped += c;
        }
        if (!stripped.empty()) {
            out += stripped;
            out += L' ';
        }
        pos = end + closeTag.size();
    }
}

ExtractionResult TextExtractor::extractOoxml(const std::wstring& filePath,
                                               const std::wstring& extension) {
    ExtractionResult res;
    res.method = L"OOXML-Direct";

    // 어떤 XML 파일에서 텍스트를 추출할지 결정
    std::vector<std::string> xmlPaths;
    std::wstring textTag;

    if (extension == L".docx") {
        xmlPaths = { "word/document.xml",
                     "word/header1.xml", "word/footer1.xml" };
        textTag  = L"w:t";
    } else if (extension == L".xlsx") {
        xmlPaths = { "xl/sharedStrings.xml" };
        textTag  = L"t";
        // 시트 xml도 추가 (숫자 셀)
        for (int i = 1; i <= 10; ++i)
            xmlPaths.push_back("xl/worksheets/sheet" + std::to_string(i) + ".xml");
    } else if (extension == L".pptx") {
        for (int i = 1; i <= 200; ++i)
            xmlPaths.push_back("ppt/slides/slide" + std::to_string(i) + ".xml");
        textTag = L"a:t";
    } else {
        res.errorMessage = L"알 수 없는 OOXML 확장자: " + extension;
        return res;
    }

    // ZIP 내 항목을 읽어 XML 텍스트 추출
    // Windows는 기본으로 zlib/deflate를 지원하지 않으므로
    // Cabinet API (FCI/FDI)를 사용하거나, Shell IZipFolder를 사용
    // 여기서는 Shell IShellDispatch를 통해 ZIP 처리
    //
    // 실용적인 대안: Windows Shell의 IStorage나 Shell 자동화를 통해 파일 추출
    // 가장 간단한 방법은 zlib을 vcpkg로 추가하는 것이지만,
    // 여기서는 Shell Namespace Extension을 이용

    // Shell을 통한 ZIP 접근 (IShellFolder)
    std::wstring text;

    // Windows Cabinet API (expand.exe 방식)보다
    // COM IShellItem + IFileOperation이 더 안정적
    // 여기서는 간단화를 위해 임시폴더에 압축 해제

    wchar_t tempDir[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, tempDir);
    std::wstring tmpFolder = std::wstring(tempDir) + L"PiiScanTmp_" +
                              std::to_wstring(GetCurrentProcessId()) + L"_" +
                              std::to_wstring(GetTickCount64());
    CreateDirectoryW(tmpFolder.c_str(), nullptr);

    // PowerShell로 압축 해제 (간단하고 신뢰성 높음)
    // Expand-Archive -Force -Path "file.docx" -DestinationPath "tmpFolder"
    std::wstring psCmd =
        L"powershell -NoProfile -NonInteractive -Command "
        L"\"Expand-Archive -Force -LiteralPath '" + filePath + L"' "
        L"-DestinationPath '" + tmpFolder + L"'\"";

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput  = INVALID_HANDLE_VALUE;
    si.hStdOutput = INVALID_HANDLE_VALUE;
    si.hStdError  = INVALID_HANDLE_VALUE;

    bool expanded = false;
    if (CreateProcessW(nullptr, psCmd.data(), nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 30000); // 최대 30초
        DWORD exitCode = 1;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        expanded = (exitCode == 0);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    if (!expanded) {
        res.errorMessage = L"OOXML 압축 해제 실패";
        // 임시 폴더 삭제
        std::wstring rmCmd = L"rd /s /q \"" + tmpFolder + L"\"";
        _wsystem(rmCmd.c_str());
        return res;
    }

    // XML 파일 읽기 및 텍스트 추출
    for (const auto& relPath : xmlPaths) {
        std::wstring wRelPath(relPath.begin(), relPath.end());
        std::wstring xmlFile = tmpFolder + L"\\" + wRelPath;

        // 파일 존재 확인
        if (GetFileAttributesW(xmlFile.c_str()) == INVALID_FILE_ATTRIBUTES) continue;

        auto xmlBytes = readFileBytes(xmlFile, 50 * 1024 * 1024); // 최대 50MB
        if (xmlBytes.empty()) continue;

        // UTF-8 XML을 wstring으로 변환
        std::wstring xmlText = toWString(xmlBytes);

        // 텍스트 태그 추출
        extractTagText(xmlText, textTag, text);

        if (text.size() > m_maxTextLength) {
            text.resize(m_maxTextLength);
            break;
        }
    }

    // 임시 폴더 삭제
    std::wstring rmCmd = L"rd /s /q \"" + tmpFolder + L"\"";
    _wsystem(rmCmd.c_str());

    if (text.empty()) {
        res.errorMessage = L"OOXML에서 텍스트를 추출하지 못함";
        return res;
    }

    res.text    = std::move(text);
    res.success = true;
    return res;
}

// ============================================================
// Windows OCR API를 이용한 이미지 텍스트 추출
// ============================================================

bool TextExtractor::ensureOcrInitialized() {
    if (m_ocrInitialized) return m_ocrAvailable;
    m_ocrInitialized = true;

    try {
        // 한국어 OCR 엔진 사용 가능 여부 확인
        auto lang = Language(L"ko-KR");
        m_ocrAvailable = OcrEngine::IsLanguageSupported(lang);
        if (!m_ocrAvailable) {
            // 영어로 fallback
            auto engLang = Language(L"en-US");
            m_ocrAvailable = OcrEngine::IsLanguageSupported(engLang);
        }
    } catch (...) {
        m_ocrAvailable = false;
    }
    return m_ocrAvailable;
}

ExtractionResult TextExtractor::extractViaWinOcr(const std::wstring& filePath) {
    ExtractionResult res;
    res.method = L"WinOCR";

    if (!ensureOcrInitialized()) {
        res.errorMessage = L"Windows OCR API를 사용할 수 없습니다. "
                           L"Windows 10 이상이며 언어팩이 설치되어 있는지 확인하세요.";
        return res;
    }

    try {
        // 한국어 우선, 없으면 영어
        OcrEngine engine = nullptr;
        try {
            engine = OcrEngine::TryCreateFromLanguage(Language(L"ko-KR"));
        } catch (...) {}
        if (!engine) {
            engine = OcrEngine::TryCreateFromLanguage(Language(L"en-US"));
        }
        if (!engine) {
            res.errorMessage = L"OCR 엔진 생성 실패";
            return res;
        }

        // 파일 로드
        auto file = StorageFile::GetFileFromPathAsync(filePath).get();
        auto stream = file.OpenAsync(FileAccessMode::Read).get();
        auto decoder = BitmapDecoder::CreateAsync(stream).get();
        auto bitmap  = decoder.GetSoftwareBitmapAsync().get();

        // OCR 실행
        auto ocrResult = engine.RecognizeAsync(bitmap).get();

        std::wstring text;
        for (const auto& line : ocrResult.Lines()) {
            text += line.Text().c_str();
            text += L'\n';
        }

        if (text.empty()) {
            res.errorMessage = L"OCR 결과가 없음 (빈 이미지 또는 텍스트 없음)";
            return res;
        }

        res.text    = std::move(text);
        res.success = true;
    } catch (const winrt::hresult_error& e) {
        res.errorMessage = L"WinRT 오류: " + std::wstring(e.message().c_str());
    } catch (const std::exception& e) {
        res.errorMessage = L"예외: ";
        // narrow to wide
        std::string msg = e.what();
        res.errorMessage += std::wstring(msg.begin(), msg.end());
    }

    return res;
}

// ============================================================
// 파일 바이트 읽기
// ============================================================

std::vector<uint8_t> TextExtractor::readFileBytes(const std::wstring& filePath,
                                                    size_t maxBytes) {
    std::ifstream ifs(filePath, std::ios::binary | std::ios::ate);
    if (!ifs) return {};

    auto fileSize = static_cast<size_t>(ifs.tellg());
    size_t readSize = (maxBytes > 0 && fileSize > maxBytes) ? maxBytes : fileSize;

    std::vector<uint8_t> buf(readSize);
    ifs.seekg(0);
    ifs.read(reinterpret_cast<char*>(buf.data()), readSize);
    buf.resize(static_cast<size_t>(ifs.gcount()));
    return buf;
}

// ============================================================
// 인코딩 감지 및 wstring 변환
// ============================================================

TextExtractor::Encoding TextExtractor::detectEncoding(
    const std::vector<uint8_t>& bytes)
{
    if (bytes.size() >= 3 &&
        bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF)
        return Encoding::UTF8;

    if (bytes.size() >= 2 &&
        bytes[0] == 0xFF && bytes[1] == 0xFE)
        return Encoding::UTF16LE;

    if (bytes.size() >= 2 &&
        bytes[0] == 0xFE && bytes[1] == 0xFF)
        return Encoding::UTF16BE;

    // 간단한 EUC-KR 휴리스틱: 0x80-0xFE 범위 바이트가 있으면 EUC-KR 가능성
    // 실제로는 더 정교한 감지 필요 (여기서는 단순화)
    size_t highBytes = 0;
    for (auto b : bytes) {
        if (b >= 0x80 && b <= 0xFE) ++highBytes;
    }
    if (highBytes > bytes.size() / 10) {
        // MultiByteToWideChar로 EUC-KR 시도
        return Encoding::EUC_KR;
    }

    return Encoding::UTF8;
}

std::wstring TextExtractor::toWString(const std::vector<uint8_t>& bytes) {
    if (bytes.empty()) return {};

    auto enc = detectEncoding(bytes);

    if (enc == Encoding::UTF16LE) {
        size_t offset = (bytes[0] == 0xFF && bytes[1] == 0xFE) ? 2 : 0;
        return std::wstring(
            reinterpret_cast<const wchar_t*>(bytes.data() + offset),
            (bytes.size() - offset) / 2
        );
    }

    if (enc == Encoding::EUC_KR) {
        return eucKrToWString(bytes);
    }

    // UTF-8 (BOM 제거)
    size_t offset = 0;
    if (bytes.size() >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF)
        offset = 3;

    int wlen = MultiByteToWideChar(CP_UTF8, 0,
        reinterpret_cast<const char*>(bytes.data() + offset),
        static_cast<int>(bytes.size() - offset),
        nullptr, 0);
    if (wlen <= 0) return {};

    std::wstring wstr(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0,
        reinterpret_cast<const char*>(bytes.data() + offset),
        static_cast<int>(bytes.size() - offset),
        wstr.data(), wlen);
    return wstr;
}

std::wstring TextExtractor::eucKrToWString(const std::vector<uint8_t>& bytes) {
    int wlen = MultiByteToWideChar(949, 0,   // 949 = EUC-KR / CP949
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<int>(bytes.size()),
        nullptr, 0);
    if (wlen <= 0) {
        // fallback UTF-8
        return toWString(bytes); // recursive once is safe
    }
    std::wstring wstr(wlen, L'\0');
    MultiByteToWideChar(949, 0,
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<int>(bytes.size()),
        wstr.data(), wlen);
    return wstr;
}
