#pragma once
// text_extractor.h
// 문서 → 텍스트 추출 (Windows IFilter)
// 이미지 → 텍스트 추출 (Windows OCR API / WinRT)

#include <windows.h>
#include <string>
#include <vector>

// -------------------------
// 추출 결과
// -------------------------
struct ExtractionResult {
    bool        success = false;
    std::wstring text;           // 추출된 텍스트
    std::wstring errorMessage;   // 실패 시 오류 메시지
    std::wstring method;         // 사용된 추출 방법 (IFilter / WinOCR / PlainText 등)
};

// -------------------------
// TextExtractor 클래스
// -------------------------
class TextExtractor {
public:
    TextExtractor();
    ~TextExtractor();

    // 파일에서 텍스트 추출 (확장자에 따라 자동 선택)
    ExtractionResult extract(const std::wstring& filePath,
                             const std::wstring& extension);

    // 최대 추출 텍스트 길이 (기본 1MB 문자)
    void setMaxTextLength(size_t maxLen) { m_maxTextLength = maxLen; }

    // Windows OCR 사용 가능 여부 (초기화 시 확인)
    bool isOcrAvailable() const { return m_ocrAvailable; }

private:
    size_t m_maxTextLength  = 1'000'000;
    bool   m_ocrAvailable   = false;
    bool   m_ocrInitialized = false;
    bool   m_winrtInited    = false;  // WinRT apartment lazy 초기화 여부

    // ---- 추출 메서드 ----

    // 일반 텍스트 파일 읽기 (UTF-8, UTF-16, EUC-KR 자동 감지)
    ExtractionResult extractPlainText(const std::wstring& filePath);

    // Windows IFilter를 이용한 문서 텍스트 추출
    // 지원: docx, xlsx, pptx, doc, xls, ppt, pdf, hwp, hwpx 등
    //       (설치된 IFilter에 따라 다름)
    ExtractionResult extractViaIFilter(const std::wstring& filePath);

    // docx/pptx/xlsx 직접 파싱 (IFilter 실패 대비 fallback)
    // OOXML은 ZIP + XML 구조
    ExtractionResult extractOoxml(const std::wstring& filePath,
                                  const std::wstring& extension);

    // Windows OCR API를 이용한 이미지 텍스트 추출
    ExtractionResult extractViaWinOcr(const std::wstring& filePath);

    // ---- 내부 유틸 ----

    // 파일을 바이트로 읽기
    std::vector<uint8_t> readFileBytes(const std::wstring& filePath,
                                        size_t maxBytes = 0);

    // 인코딩 감지 후 wstring으로 변환
    std::wstring toWString(const std::vector<uint8_t>& bytes);

    // BOM 감지
    enum class Encoding { Unknown, UTF8, UTF16LE, UTF16BE, EUC_KR };
    Encoding detectEncoding(const std::vector<uint8_t>& bytes);

    // EUC-KR → wstring 변환
    std::wstring eucKrToWString(const std::vector<uint8_t>& bytes);

    // OOXML ZIP 파싱 헬퍼: xmlPath 내 텍스트 태그 추출
    bool extractXmlText(const std::vector<uint8_t>& xmlBytes,
                        std::wstring& outText,
                        const std::vector<std::wstring>& textTags);

    // ZIP 파일에서 특정 엔트리 읽기 (minizip 없이 CRC만 검증하는 간단 구현)
    bool readZipEntry(const std::wstring& zipPath,
                      const std::string& entryName,
                      std::vector<uint8_t>& outData);

    // OCR 초기화 확인
    bool ensureOcrInitialized();
};
