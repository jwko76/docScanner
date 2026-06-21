"""OCR 검증용 테스트 이미지 생성 스크립트"""
from PIL import Image, ImageDraw, ImageFont
import os

out_dir = r"D:\piiscan_test\ocr_test"
os.makedirs(out_dir, exist_ok=True)

def make_image(filename, lines, size=(800, 400), bg="white", fg="black", fontsize=28):
    img = Image.new("RGB", size, bg)
    draw = ImageDraw.Draw(img)
    # Windows 기본 폰트 시도
    fonts_to_try = [
        r"C:\Windows\Fonts\malgun.ttf",      # 맑은 고딕 (한글)
        r"C:\Windows\Fonts\gulim.ttc",        # 굴림
        r"C:\Windows\Fonts\batang.ttc",       # 바탕
        r"C:\Windows\Fonts\arial.ttf",        # Arial (영문)
    ]
    font = None
    for fp in fonts_to_try:
        try:
            font = ImageFont.truetype(fp, fontsize)
            break
        except:
            continue
    if font is None:
        font = ImageFont.load_default()

    y = 30
    for line in lines:
        draw.text((40, y), line, fill=fg, font=font)
        y += fontsize + 12

    path = os.path.join(out_dir, filename)
    img.save(path)
    print(f"  생성: {path}")
    return path

print("=== OCR 테스트 이미지 생성 ===")

# 1. 주민등록번호 + 전화번호 (PNG)
make_image("test_rrn_phone.png", [
    "환자 정보",
    "성명: 홍길동",
    "주민등록번호: 850101-1234566",
    "전화번호: 010-1234-5678",
    "주소: 서울특별시 강남구 테헤란로 123",
])

# 2. 이메일 + 계좌번호 (JPG)
make_image("test_email_account.jpg", [
    "이체 확인서",
    "이메일: hong@example.com",
    "계좌번호: 110-1234-567890",
    "신용카드: 1234-5678-9012-3456",
    "발급일: 2024-01-15",
])

# 3. 스캔 문서처럼 회색 배경 + 약간 기울어진 느낌 (PNG)
make_image("test_scanned_doc.png", [
    "개인정보 동의서",
    "여권번호: M12345678",
    "운전면허: 12-34-567890-01",
    "이메일: test.user@company.co.kr",
    "연락처: 02-1234-5678",
], bg=(230, 230, 230))

# 4. 작은 폰트 (실제 스캔 문서처럼)
make_image("test_small_font.png", [
    "거래명세서",
    "주민번호: 920315-2345678",
    "계좌: 우리은행 1002-123-456789",
    "카드번호: 4111-1111-1111-1111",
    "담당자: test@company.com",
], size=(1000, 500), fontsize=22)

print("\n총 4개 이미지 생성 완료")
print(f"저장 위치: {out_dir}")
