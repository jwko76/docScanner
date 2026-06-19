// pii_detector.cpp
// 개인정보 탐지 패턴 구현

#include <regex>           // std::wregex, std::wsregex_iterator (explicit include)
#include "pii_detector.h"
#include <algorithm>
#include <numeric>
#include <cctype>
#include <sstream>

// ============================================================
// 유형명 / 마스킹
// ============================================================

std::wstring PiiDetector::getTypeName(PiiType type) {
    switch (type) {
        case PiiType::RESIDENT_NUMBER:  return L"주민등록번호";
        case PiiType::PHONE_NUMBER:     return L"전화번호";
        case PiiType::EMAIL:            return L"이메일";
        case PiiType::IP_ADDRESS:       return L"IP 주소";
        case PiiType::MAC_ADDRESS:      return L"MAC 주소";
        case PiiType::CREDIT_CARD:      return L"신용카드번호";
        case PiiType::ADDRESS:          return L"주소";
        case PiiType::BANK_ACCOUNT:     return L"계좌번호";
        case PiiType::PASSPORT:         return L"여권번호";
        case PiiType::DRIVER_LICENSE:   return L"운전면허번호";
        default:                        return L"기타";
    }
}

std::wstring PiiDetector::getMaskPattern(PiiType type, const std::wstring& text) {
    switch (type) {
        case PiiType::RESIDENT_NUMBER: {
            // 850101-1234567 → 850101-*******
            auto hyphen = text.find(L'-');
            if (hyphen != std::wstring::npos)
                return text.substr(0, hyphen + 1) + L"*******";
            return text.substr(0, 6) + L"-*******";
        }
        case PiiType::CREDIT_CARD: {
            // 마지막 4자리만 남기기 (숫자만 추출)
            std::wstring digits;
            for (wchar_t c : text) if (iswdigit(c)) digits += c;
            if (digits.size() >= 4)
                return L"****-****-****-" + digits.substr(digits.size() - 4);
            return L"****-****-****-****";
        }
        case PiiType::EMAIL: {
            // id@domain → i***@domain
            auto at = text.find(L'@');
            if (at == std::wstring::npos) return L"***@***";
            std::wstring id = text.substr(0, at);
            std::wstring domain = text.substr(at);
            if (id.size() <= 1) return L"*" + domain;
            return id.substr(0, 1) + std::wstring(id.size() - 1, L'*') + domain;
        }
        case PiiType::PHONE_NUMBER: {
            // 010-1234-5678 → 010-****-5678
            auto parts = text;
            // 중간 4자리 마스킹
            std::wstring result = text;
            // 간단 구현: 두 번째 '-' 앞뒤 마스킹
            int dashCount = 0;
            bool masking = false;
            for (size_t i = 0; i < result.size(); ++i) {
                if (result[i] == L'-') {
                    ++dashCount;
                    if (dashCount == 1) masking = true;
                    if (dashCount == 2) masking = false;
                } else if (masking && iswdigit(result[i])) {
                    result[i] = L'*';
                }
            }
            return result;
        }
        default:
            // 절반 마스킹
            if (text.size() <= 4) return std::wstring(text.size(), L'*');
            size_t half = text.size() / 2;
            return text.substr(0, half) + std::wstring(text.size() - half, L'*');
    }
}

// ============================================================
// 생성자
// ============================================================

PiiDetector::PiiDetector() {
    initPatterns();
    // 기본 모두 활성화
    for (auto type : {
            PiiType::RESIDENT_NUMBER, PiiType::PHONE_NUMBER,
            PiiType::EMAIL, PiiType::IP_ADDRESS, PiiType::MAC_ADDRESS,
            PiiType::CREDIT_CARD, PiiType::ADDRESS,
            PiiType::BANK_ACCOUNT, PiiType::PASSPORT, PiiType::DRIVER_LICENSE
        }) {
        m_enabled[type] = true;
    }
}

void PiiDetector::setEnabled(PiiType type, bool enabled) {
    m_enabled[type] = enabled;
    for (auto& p : m_patterns) {
        if (p.type == type) p.enabled = enabled;
    }
}

bool PiiDetector::isEnabled(PiiType type) const {
    auto it = m_enabled.find(type);
    return it != m_enabled.end() ? it->second : true;
}

// ============================================================
// 패턴 초기화
// ============================================================

void PiiDetector::initPatterns() {
    // 주민등록번호: YYMMDD-NNNNNNN
    // 뒷자리 첫 번째: 1-4 (1900년대 남성=1, 여성=2, 2000년대 남성=3, 여성=4)
    // 또는 외국인 등록번호: 5-8
    m_patterns.push_back({
        PiiType::RESIDENT_NUMBER,
        std::wregex(LR"(\b(\d{6})([-\s])([1-489]\d{6})\b)"),
        true, true  // 체크섬 검증
    });

    // 전화번호 (국내):
    //   - 010/011/016/017/018/019-XXXX-XXXX
    //   - 02-XXXX-XXXX, 031-XXXX-XXXX 등
    //   - 1588-XXXX, 1800-XXXX 등 대표번호
    m_patterns.push_back({
        PiiType::PHONE_NUMBER,
        std::wregex(
            LR"(\b(0(?:1[016-9]|2|3[1-3]|4[1-4]|5[1-5]|6[1-4]|7[1-4]|8[0-2]))"
            LR"(([-\s.\)])(\d{3,4})([-\s.\)])(\d{4})\b)"
            LR"(|\b(1[5-8]\d{2})([-\s.\)])(\d{4})\b)"
        ),
        true, false
    });

    // 이메일
    m_patterns.push_back({
        PiiType::EMAIL,
        std::wregex(
            LR"(\b[a-zA-Z0-9._%+\-]+@[a-zA-Z0-9.\-]+\.[a-zA-Z]{2,}\b)"
        ),
        true, false
    });

    // IPv4 주소 (사설/공인 포함)
    m_patterns.push_back({
        PiiType::IP_ADDRESS,
        std::wregex(
            LR"(\b((25[0-5]|2[0-4]\d|[01]?\d\d?)\.){3}(25[0-5]|2[0-4]\d|[01]?\d\d?)\b)"
        ),
        true, true  // 0.0.0.0, 255.255.255.255 등 제외
    });

    // MAC 주소 (XX:XX:XX:XX:XX:XX 또는 XX-XX-XX-XX-XX-XX)
    m_patterns.push_back({
        PiiType::MAC_ADDRESS,
        std::wregex(
            LR"(\b([0-9A-Fa-f]{2}[:\-]){5}[0-9A-Fa-f]{2}\b)"
        ),
        true, false
    });

    // 신용카드 번호 (16자리, 구분자 허용)
    // Visa, Master, AMEX(15자리), 등
    m_patterns.push_back({
        PiiType::CREDIT_CARD,
        std::wregex(
            // 4자리-4자리-4자리-4자리
            LR"(\b(?:4[0-9]{3}|5[1-5][0-9]{2}|3[47][0-9]{2}|6(?:011|5[0-9]{2}))"
            LR"([\ \-]?\d{4}[\ \-]?\d{4}[\ \-]?\d{4}\b)"
            // 또는 단순 16자리 연속 숫자 (오탐 가능성 있음)
            LR"(|\b\d{4}[\s\-]?\d{4}[\s\-]?\d{4}[\s\-]?\d{4}\b)"
        ),
        true, true  // Luhn 검증
    });

    // 계좌번호: 국내 주요 은행 패턴
    // 농협: 12자리, 국민: 14자리, 신한: 11자리 등
    m_patterns.push_back({
        PiiType::BANK_ACCOUNT,
        std::wregex(
            LR"(\b\d{3,4}[-\s]?\d{4,6}[-\s]?\d{2,7}\b)"
        ),
        true, false
    });

    // 여권번호: 영문 2자리 + 숫자 7자리
    m_patterns.push_back({
        PiiType::PASSPORT,
        std::wregex(LR"(\b[A-Z]{2}\d{7}\b)"),
        true, false
    });

    // 운전면허번호: 지역코드-생년-일련번호-검증코드
    // 예: 12-00-000000-00
    m_patterns.push_back({
        PiiType::DRIVER_LICENSE,
        std::wregex(
            LR"(\b(0[1-9]|[1-2]\d|3[0-6])-(\d{2})-(\d{6})-(\d{2})\b)"
        ),
        true, false
    });
}

// ============================================================
// 탐지 실행
// ============================================================

std::vector<PiiMatch> PiiDetector::detect(const std::wstring& text) {
    std::vector<PiiMatch> allMatches;

    for (const auto& pat : m_patterns) {
        if (!pat.enabled) continue;
        if (!isEnabled(pat.type)) continue;
        if (allMatches.size() >= m_maxMatches) break;

        std::wsregex_iterator it(text.begin(), text.end(), pat.pattern);
        std::wsregex_iterator end;

        for (; it != end && allMatches.size() < m_maxMatches; ++it) {
            const auto& m = *it;
            std::wstring matched = m.str();
            size_t pos = static_cast<size_t>(m.position());

            // 추가 검증
            float confidence = 0.8f;
            if (pat.doValidate) {
                switch (pat.type) {
                    case PiiType::RESIDENT_NUMBER:
                        if (!validateResidentNumber(matched)) continue;
                        confidence = 0.99f;
                        break;
                    case PiiType::CREDIT_CARD:
                        if (!validateCreditCard(matched)) continue;
                        confidence = 0.95f;
                        break;
                    case PiiType::IP_ADDRESS:
                        if (!validateIpAddress(matched)) continue;
                        confidence = 0.9f;
                        break;
                    default:
                        break;
                }
            }

            PiiMatch pm;
            pm.type           = pat.type;
            pm.typeName       = getTypeName(pat.type);
            pm.matchedText    = matched;
            pm.maskedText     = getMaskPattern(pat.type, matched);
            pm.position       = pos;
            pm.lineNumber     = getLineNumber(text, pos);
            pm.contextSnippet = extractContext(text, pos, matched.size());
            pm.confidence     = confidence;

            allMatches.push_back(std::move(pm));
        }
    }

    // 주소 탐지 (키워드 기반)
    if (isEnabled(PiiType::ADDRESS) && allMatches.size() < m_maxMatches) {
        auto addrMatches = detectAddress(text);
        for (auto& am : addrMatches) {
            if (allMatches.size() >= m_maxMatches) break;
            allMatches.push_back(std::move(am));
        }
    }

    // 위치 순 정렬
    std::sort(allMatches.begin(), allMatches.end(),
        [](const PiiMatch& a, const PiiMatch& b) {
            return a.position < b.position;
        });

    return allMatches;
}

// ============================================================
// 주민등록번호 체크섬 검증
// ============================================================

bool PiiDetector::validateResidentNumber(const std::wstring& rn) {
    // 숫자만 추출
    std::wstring digits;
    for (wchar_t c : rn) {
        if (iswdigit(c)) digits += c;
    }
    if (digits.size() != 13) return false;

    // 날짜 유효성 (앞 6자리)
    int year  = std::stoi(digits.substr(0, 2));
    int month = std::stoi(digits.substr(2, 2));
    int day   = std::stoi(digits.substr(4, 2));

    if (month < 1 || month > 12) return false;
    if (day   < 1 || day   > 31) return false;

    // 체크섬
    // 가중치: 2,3,4,5,6,7,8,9,2,3,4,5
    const int weights[] = {2,3,4,5,6,7,8,9,2,3,4,5};
    int sum = 0;
    for (int i = 0; i < 12; ++i) {
        sum += (digits[i] - L'0') * weights[i];
    }
    int checkDigit = (11 - (sum % 11)) % 10;
    return checkDigit == (digits[12] - L'0');
}

// ============================================================
// 신용카드 Luhn 알고리즘 검증
// ============================================================

bool PiiDetector::validateCreditCard(const std::wstring& cc) {
    std::wstring digits;
    for (wchar_t c : cc) {
        if (iswdigit(c)) digits += c;
    }
    if (digits.size() < 13 || digits.size() > 19) return false;

    // Luhn 알고리즘
    int sum = 0;
    bool alternate = false;
    for (int i = (int)digits.size() - 1; i >= 0; --i) {
        int n = digits[i] - L'0';
        if (alternate) {
            n *= 2;
            if (n > 9) n -= 9;
        }
        sum += n;
        alternate = !alternate;
    }
    return (sum % 10 == 0);
}

// ============================================================
// IP 주소 유효성 검증 (루프백, 브로드캐스트 등 제외)
// ============================================================

bool PiiDetector::validateIpAddress(const std::wstring& ip) {
    // 버전 번호 패턴 제외: 1.0.0, 2.0.0 등 주요 소프트웨어 버전
    // 간단한 필터: 모든 옥텟이 0이거나 255이면 제외
    std::vector<int> octets;
    std::wstringstream ss(ip);
    std::wstring token;
    while (std::getline(ss, token, L'.')) {
        try { octets.push_back(std::stoi(token)); }
        catch (...) { return false; }
    }
    if (octets.size() != 4) return false;

    // 0.0.0.0, 255.255.255.255 제외
    if (octets[0] == 0 && octets[1] == 0 && octets[2] == 0 && octets[3] == 0)
        return false;
    if (octets[0] == 255 && octets[1] == 255)
        return false;

    // 버전 번호 패턴 제외 (예: 1.0.0.1 ~ 9.9.9.9)
    // 너무 단순한 숫자는 버전일 가능성 높음
    bool allSmall = true;
    for (int o : octets) {
        if (o > 9) { allSmall = false; break; }
    }
    // 모두 한 자리이면 버전 번호일 가능성 높으므로 제외 (선택적)
    // 여기서는 포함

    return true;
}

// ============================================================
// 한국 주소 탐지 (키워드 기반)
// ============================================================

std::vector<PiiMatch> PiiDetector::detectAddress(const std::wstring& text) {
    std::vector<PiiMatch> results;

    // 한국 주소 패턴:
    // - 시/도 + 구/군 + 동/면/읍 + 번지
    // - 도로명 주소: 시 + 구 + 로/길 + 번호
    std::wregex addrPattern(
        // 광역시/도 + 구/군 + 동/읍/면/로/길
        LR"((서울|부산|대구|인천|광주|대전|울산|세종|경기|강원|충북|충남|전북|전남|경북|경남|제주)"
        LR"([\s\S]{0,5}?)"
        LR"((시|도|구|군|동|읍|면|로|길|가|번지|번|호|아파트|빌라|오피스텔))"
        LR"([\s\S]{0,20}?\d+)"  // 번지/호수
        LR"([\s\S]{0,10}?(동|호|층)?)"
    );

    std::wsregex_iterator it(text.begin(), text.end(), addrPattern);
    std::wsregex_iterator end;

    for (; it != end && results.size() < m_maxMatches / 10; ++it) {
        const auto& m = *it;
        std::wstring matched = m.str();
        size_t pos = static_cast<size_t>(m.position());

        // 너무 짧으면 제외
        if (matched.size() < 8) continue;

        PiiMatch pm;
        pm.type           = PiiType::ADDRESS;
        pm.typeName       = L"주소";
        pm.matchedText    = matched;
        pm.maskedText     = matched.substr(0, matched.size() / 2) +
                            std::wstring(matched.size() - matched.size() / 2, L'*');
        pm.position       = pos;
        pm.lineNumber     = getLineNumber(text, pos);
        pm.contextSnippet = extractContext(text, pos, matched.size());
        pm.confidence     = 0.7f;

        results.push_back(std::move(pm));
    }

    return results;
}

// ============================================================
// 내부 유틸
// ============================================================

std::wstring PiiDetector::extractContext(const std::wstring& text,
                                          size_t pos, size_t len) {
    size_t halfCtx = m_contextLen / 2;
    size_t start = (pos > halfCtx) ? pos - halfCtx : 0;
    size_t end   = std::min(pos + len + halfCtx, text.size());

    std::wstring ctx = text.substr(start, end - start);

    // 줄바꿈을 공백으로 치환
    std::replace(ctx.begin(), ctx.end(), L'\n', L' ');
    std::replace(ctx.begin(), ctx.end(), L'\r', L' ');
    std::replace(ctx.begin(), ctx.end(), L'\t', L' ');

    return ctx;
}

int PiiDetector::getLineNumber(const std::wstring& text, size_t pos) {
    int lineNum = 1;
    size_t limit = std::min(pos, text.size());
    for (size_t i = 0; i < limit; ++i) {
        if (text[i] == L'\n') ++lineNum;
    }
    return lineNum;
}

std::wstring PiiDetector::normalizeNumber(const std::wstring& s) {
    std::wstring out;
    for (wchar_t c : s) {
        if (iswdigit(c)) out += c;
    }
    return out;
}

// ============================================================
// FileScanResult 메서드
// ============================================================

int FileScanResult::countByType(PiiType t) const {
    return (int)std::count_if(matches.begin(), matches.end(),
        [t](const PiiMatch& m) { return m.type == t; });
}
