#pragma once
// pii_detector.h
// 개인정보 / 민감정보 탐지 모듈
// 주민등록번호, 전화번호, 이메일, IP/MAC, 신용카드, 주소 패턴 탐지

#include <string>
#include <vector>
#include <regex>
#include <map>

// -------------------------
// 탐지 유형
// -------------------------
enum class PiiType {
    RESIDENT_NUMBER,  // 주민등록번호
    PHONE_NUMBER,     // 전화번호 (국내)
    EMAIL,            // 이메일 주소
    IP_ADDRESS,       // IPv4 주소
    MAC_ADDRESS,      // MAC 주소
    CREDIT_CARD,      // 신용카드 번호
    ADDRESS,          // 한국 주소 (키워드 기반)
    BANK_ACCOUNT,     // 계좌번호
    PASSPORT,         // 여권번호
    DRIVER_LICENSE,   // 운전면허번호
};

// -------------------------
// 탐지 결과 항목
// -------------------------
struct PiiMatch {
    PiiType       type;
    std::wstring  typeName;       // 한국어 유형명
    std::wstring  matchedText;    // 탐지된 원문
    std::wstring  maskedText;     // 마스킹된 문자열 (예: 850101-*******)
    size_t        position;       // 텍스트 내 위치
    int           lineNumber;     // 줄 번호
    std::wstring  contextSnippet; // 전후 맥락 (최대 100자)
    float         confidence;     // 신뢰도 (0~1, 체크섬 검증 등 반영)
};

// -------------------------
// 파일 스캔 결과
// -------------------------
struct FileScanResult {
    std::wstring           filePath;
    std::wstring           extension;
    bool                   extractionSuccess;
    std::wstring           extractionError;
    std::wstring           extractionMethod;
    size_t                 textLength;       // 추출된 텍스트 길이
    double                 scanTimeSec;      // 스캔 소요 시간
    std::vector<PiiMatch>  matches;

    // 유형별 건수 조회
    int countByType(PiiType t) const;
    int totalMatches() const { return (int)matches.size(); }
};

// -------------------------
// PiiDetector 클래스
// -------------------------
class PiiDetector {
public:
    PiiDetector();

    // 텍스트에서 개인정보 탐지
    std::vector<PiiMatch> detect(const std::wstring& text);

    // 유형별 활성화/비활성화
    void setEnabled(PiiType type, bool enabled);
    bool isEnabled(PiiType type) const;

    // 컨텍스트 스니펫 길이 설정 (기본 100자)
    void setContextLength(size_t len) { m_contextLen = len; }

    // 최대 탐지 건수 (성능 보호)
    void setMaxMatches(size_t max) { m_maxMatches = max; }

    // 유형명 조회
    static std::wstring getTypeName(PiiType type);
    static std::wstring getMaskPattern(PiiType type, const std::wstring& text);

private:
    struct PatternDef {
        PiiType      type;
        std::wregex  pattern;
        bool         enabled = true;
        bool         doValidate = false;  // 추가 체크섬 검증 여부
    };

    std::vector<PatternDef> m_patterns;
    std::map<PiiType, bool> m_enabled;
    size_t m_contextLen  = 100;
    size_t m_maxMatches  = 10000;

    void initPatterns();

    // 검증 함수
    bool validateResidentNumber(const std::wstring& rn);  // 체크섬 + 날짜 유효성
    bool validateCreditCard(const std::wstring& cc);       // Luhn 알고리즘
    bool validateIpAddress(const std::wstring& ip);        // 0.0.0.0, 255.255.255.255 등 제외

    // 주소 키워드 탐지 (정규식 보완)
    std::vector<PiiMatch> detectAddress(const std::wstring& text);

    // 컨텍스트 추출
    std::wstring extractContext(const std::wstring& text, size_t pos, size_t len);

    // 줄 번호 계산
    int getLineNumber(const std::wstring& text, size_t pos);

    // 텍스트 정규화 (숫자/하이픈만 추출)
    static std::wstring normalizeNumber(const std::wstring& s);
};
