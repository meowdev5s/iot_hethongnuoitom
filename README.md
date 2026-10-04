# HỆ THỐNG QUẢN LÝ TRANG TRẠI NUÔI TÔM THÔNG MINH (SMART SHRIMP FARM)

### Đề tài 16 — Nhóm 2

Dự án ứng dụng IoT giám sát chất lượng nước ao tôm theo thời gian thực kết hợp thuật toán điều khiển an toàn sinh học cục bộ (Edge Computing Interlock) và công cụ mô phỏng số (Digital Twin Testbench).

---

## MỤC LỤC

1. [Kiến trúc hệ thống](#1-kiến-trúc-hệ-thống)
2. [Cấu trúc thư mục dự án](#2-cấu-trúc-thư-mục-dự-án)
3. [Yêu cầu phần mềm cài đặt](#3-yêu-cầu-phần-mềm-cài-đặt)
4. [Hướng dẫn cài đặt và chạy thử (Trong 3 phút)](#4-hướng-dẫn-cài-đặt-và-chạy-thử-trong-3-phút)
   - [Cách 1: Chạy mô phỏng trên VS Code (Khuyên dùng)](#cách-1-chạy-mô-phỏng-trực-tiếp-trên-vs-code-khuyên-dùng)
   - [Cách 2: Chạy trực tiếp trên Web Wokwi.com (Không cần cài Extension)](#cách-2-chạy-trực-tiếp-trên-web-wokwicom)
5. [Hướng dẫn kiểm thử 4 Kịch bản nghiệm thu](#5-hướng-dẫn-kiểm-thử-4-kịch-bản-nghiệm-thu)
6. [Bảng phân bổ chân GPIO phần cứng](#6-bảng-phân-bổ-chân-gpio-phần-cứng)
7. [Cây Topic MQTT Cloud](#7-cây-topic-mqtt-cloud)
8. [Xử lý sự cố thường gặp (Troubleshooting)](#8-xử-lý-sự-cố-thường-gặp-troubleshooting)

---

## 1. KIẾN TRÚC HỆ THỐNG

Hệ thống hoạt động theo mô hình phân tầng chuẩn công nghiệp (3-Tier Architecture):

- **Tầng 1 (Edge Node - Wokwi ESP32):** Thu thập dữ liệu từ cảm biến nhiệt độ DS18B20, biến trở giả lập DO, pH, Độ mặn, Dòng quạt và cảm biến siêu âm HC-SR04; thực thi thuật toán Khóa liên động (Interlock) tự động bật 100% dàn quạt và khóa máy cho ăn khi sụt Oxy.
- **Tầng 2 (Transport / Cloud Broker):** Máy chủ đám mây HiveMQ Public Broker (`broker.hivemq.com`), hỗ trợ TCP port `1883` cho ESP32 và WebSocket Secure port `8884` (`wss://`) cho trình duyệt Web.
- **Tầng 3 (Application / Web SCADA & Digital Twin):** Giao diện điều khiển 2 chiều, hiển thị 6 thẻ thông số thời gian thực, đồ thị biến thiên DO/pH (Chart.js) và bộ thanh trượt Digital Twin Testbench giả lập tham số từ xa.

---

## 2. CẤU TRÚC THƯ MỤC DỰ ÁN

```text
MoPhong_AoTom_Wokwi/
├── src/
│   └── main.cpp           # Mã nguồn C++ ESP32 (Đọc cảm biến, Interlock, MQTT Client, Sandbox)
├── web/
│   └── index.html          # Giao diện Web Dashboard SCADA + Digital Twin Testbench (All-in-one)
├── diagram.json            # Sơ đồ mạch điện Wokwi (ESP32 + 13 ngoại vi kết nối chuẩn hóa)
├── platformio.ini          # Cấu hình nạp PlatformIO (Espressif32, tự tải đủ 6 thư viện)
├── wokwi.toml              # File cấu hình liên kết Wokwi với file binary firmware.bin
├── libraries.txt           # Danh mục thư viện dùng khi import lên web Wokwi.com
└── README.md               # Tài liệu hướng dẫn sử dụng và kiểm thử dự án
```

---

## 3. YÊU CẦU PHẦN MỀM CÀI ĐẶT

Chỉ cần máy tính có kết nối Internet và các phần mềm sau:

1. **Visual Studio Code (VS Code)**: Tải tại [code.visualstudio.com](https://code.visualstudio.com/).
2. **Extensions cần cài trên VS Code:**
   - **PlatformIO IDE** (của PlatformIO): Dùng để biên dịch mã nguồn C++ ESP32.
   - **Wokwi Simulator** (của Wokwi): Dùng để chạy mô phỏng mạch điện tử ngay trong VS Code.
3. **Trình duyệt Web hiện đại:** Google Chrome, Microsoft Edge, Brave hoặc Firefox.

*(Lưu ý: Dự án thiết kế Web Client chạy thuần HTML5/JS giao tiếp trực tiếp qua WebSocket, **không cần cài Node.js, npm hay Python server**).*

---

## 4. HƯỚNG DẪN CÀI ĐẶT VÀ CHẠY THỬ (TRONG 3 PHÚT)

### Cách 1: Chạy mô phỏng trực tiếp trên VS Code (Khuyên dùng)

#### Bước 1: Mở dự án trong VS Code

- Khởi động VS Code.
- Chọn `File` -> `Open Folder...` -> Trỏ đến thư mục `MoPhong_AoTom_Wokwi`.

#### Bước 2: Biên dịch Firmware (PlatformIO)

- Nhìn xuống thanh trạng thái màu xanh dưới cùng của VS Code:
  - Nhấp vào biểu tượng **dấu tích (✓)** (`PlatformIO: Build`) để biên dịch.
  - *Hoặc:* Mở tab Terminal trong VS Code và gõ lệnh:
    ```bash
    pio run
    ```
- Chờ PlatformIO tự động tải các thư viện (`PubSubClient`, `ArduinoJson`, `LiquidCrystal_I2C`, `DallasTemperature`, `ESP32Servo`) và hiển thị thông báo `[SUCCESS]`.
- File firmware sẽ được tự động tạo tại: `.pio/build/az-delivery-devkit-v4/firmware.bin`.

#### Bước 3: Khởi chạy mô phỏng mạch Wokwi

- Mở file `diagram.json` trong cây thư mục VS Code.
- Trên góc giao diện sơ đồ mạch, bấm nút **Start Simulation** (hình tam giác màu xanh) hoặc nhấn phím tắt `F1` -> gõ `Wokwi: Start Simulation`.
- Quan sát:
  - ESP32 kết nối mạng Wi-Fi ảo `Wokwi-GUEST`.
  - Kết nối tới Cloud MQTT Broker.
  - Màn hình LCD 20x4 sáng đèn và hiển thị thông số khởi động của ao nuôi.

#### Bước 4: Mở giao diện Web Dashboard

- Tìm đến file `web/index.html` trong thư mục dự án.
- **Cách đơn giản nhất:** Chuột phải vào file `index.html` -> Chọn `Reveal in File Explorer` -> Click đúp chuột để mở trực tiếp trên trình duyệt Chrome/Edge.
- *Hoặc nếu có extension Live Server:* Chuột phải vào `index.html` -> Chọn `Open with Live Server`.
- Quan sát góc trên bên phải trang Web:
  - Đèn báo mạng: `MQTT: ONLINE (WSS:8884)` (Xanh lá).
  - Đèn báo trạm: `TRẠM AO: ONLINE` (Khi Wokwi đang chạy).
  - 6 thẻ chỉ số và đồ thị bắt đầu cập nhật dữ liệu tự động.

---

### Cách 2: Chạy trực tiếp trên Web Wokwi.com

Nếu bạn không muốn cài đặt VS Code hay PlatformIO:

1. Mở trình duyệt truy cập: [wokwi.com/projects/new/esp32](https://wokwi.com/projects/new/esp32).
2. Copy toàn bộ nội dung file `diagram.json` dán đè vào tab `diagram.json` trên web Wokwi.
3. Copy toàn bộ nội dung file `src/main.cpp` dán đè vào tab `sketch.ino` (hoặc `main.cpp`).
4. Tại tab **Library Manager** (hoặc file `libraries.txt`), thêm đủ các thư viện:
   - `LiquidCrystal I2C`
   - `OneWire`
   - `DallasTemperature`
   - `ESP32Servo`
   - `PubSubClient`
   - `ArduinoJson`
5. Nhấn nút **Play (Start Simulation)** trên web Wokwi.
6. Mở file `web/index.html` trên máy tính bằng trình duyệt. Hai bên sẽ tự động bắt tay qua Cloud MQTT của HiveMQ!

---

## 5. HƯỚNG DẪN KIỂM THỬ 4 KỊCH BẢN NGHIỆM THU

Giao diện Web cung cấp bảng **Digital Twin Testbench** bên phải để kiểm thử các tình huống thực tế theo Chương 5 của Báo cáo:

### Kịch bản 1: Vận hành bình thường (Môi trường lý tưởng)

- **Thao tác:** Bấm nút **`KB 1: VẬN HÀNH BÌNH THƯỜNG`** trên Web.
- **Tham số nạp:** $DO = 5.2$ mg/L, $pH = 7.8$, Nhiệt độ $= 28.5^\circ\text{C}$, Dòng quạt $= 9.5$ A.
- **Phản ứng hệ thống:**
  - Trên Web: Cả 6 thẻ thông số hiển thị màu xanh an toàn, đồ thị đo ổn định.
  - Trên Wokwi: Quạt chính (Relay 1) bật, Quạt phụ (Relay 2) tắt để tiết kiệm điện, Đèn LED Xanh sáng, Còi không kêu, Động cơ Servo định kỳ xả thức ăn.

### Kịch bản 2: Báo động sự cố Sụt Oxy cấp tính ban đêm (Khóa liên động Interlock)

- **Thao tác:** Bấm nút màu đỏ **`KB 2: SỤT OXY BAN ĐÊM`** (hoặc kéo thanh trượt DO xuống mức $2.8$ mg/L).
- **Phản ứng hệ thống:**
  - **Trên Web:** Bật ngay Banner cảnh báo sự cố khẩn cấp trên đầu trang, phát âm thanh chuông báo động, thẻ Oxy chuyển sang nhấp nháy đỏ, nút bấm cho ăn bị khóa cứng.
  - **Trên Wokwi:** Còi Buzzer hú liên tục, Đèn LED Đỏ bật sáng, Relay 1 và Relay 2 đồng loạt kích hoạt chạy 100% công suất quạt để cấp cứu Oxy cho tôm. Động cơ Servo máy cho ăn bị khóa cứng tại góc $0^\circ$ (triệt tiêu nguy cơ thối đáy ao). Màn hình LCD hiện thông báo `!! NGUY HIEM: SUT OXY`.

### Kịch bản 3: Báo động Quạt kẹt cánh / Quá tải dòng điện mô tơ

- **Thao tác:** Bấm nút màu vàng **`KB 3: QUẠT KẸT/QUÁ TẢI`** (Dòng điện vọt lên $18.5$ A $> 16.0$ A).
- **Phản ứng hệ thống:**
  - Trên Web: Thẻ đo dòng điện chuyển sang cảnh báo đỏ, hộp Audit Trail Log ghi nhận lỗi nguy cơ cháy động cơ.
  - Trên Wokwi: Màn hình LCD hiện cảnh báo `!! QUAT QUA TAI CAP !!`, còi kêu báo sự cố kỹ thuật.

### Kịch bản 4: Can thiệp và Điều khiển 2 chiều từ xa

- **Thao tác:** Trong điều kiện môi trường bình thường, bấm các nút trên khung **"ĐIỀU KHIỂN & TRẠNG THÁI THIẾT BỊ"**:
  - Bấm nút `QUẠT 1` -> Relay 1 trên Wokwi đóng/ngắt tương ứng.
  - Bấm nút `QUẠT 2` -> Relay 2 trên Wokwi đóng/ngắt tương ứng.
  - Bấm nút `XẢ THỨC ĂN (2S)` -> Động cơ Servo trên Wokwi quay $90^\circ$ mở nắp thức ăn trong 2 giây rồi tự động đóng lại.
- **Lưu ý an toàn:** Khi đang ở luồng khẩn cấp (Sụt Oxy), hệ thống sẽ từ chối mọi lệnh tắt quạt từ xa để bảo vệ đàn tôm.

---

## 6. BẢNG PHÂN BỔ CHÂN GPIO PHẦN CỨNG

Sơ đồ kết nối đã được tối ưu, sử dụng hoàn toàn khối **ADC1** (không bị tranh chấp khi bật Wi-Fi):

| Linh kiện                      |         Chân ESP32         | Loại chân / Giao thức | Chức năng chi tiết                                       |
| :------------------------------ | :--------------------------: | :----------------------: | :---------------------------------------------------------- |
| **DS18B20**               |            GPIO 4            |    Digital (OneWire)    | Đo nhiệt độ nước ao nuôi                             |
| **Biến trở DO**         |           GPIO 34           |    Analog (ADC1_CH6)    | Giả lập cảm biến Oxy hòa tan ($0 - 10$ mg/L)         |
| **Biến trở pH**         |           GPIO 35           |    Analog (ADC1_CH7)    | Giả lập cảm biến độ pH nước ($0 - 14$ pH)         |
| **Biến trở Salinity**   |           GPIO 32           |    Analog (ADC1_CH4)    | Giả lập cảm biến độ mặn ($0 - 35$ ppt)             |
| **Biến trở CT (Dòng)** |           GPIO 33           |    Analog (ADC1_CH5)    | Giả lập biến dòng đo tải mô tơ quạt ($0 - 20$ A) |
| **HC-SR04 (Trig)**        |            GPIO 5            |      Digital Output      | Phát xung siêu âm đo khoảng cách mực nước          |
| **HC-SR04 (Echo)**        |           GPIO 18           |      Digital Input      | Nhận phản hồi sóng siêu âm                            |
| **LCD 20x4 I2C**          | GPIO 21 (SDA), GPIO 22 (SCL) | I2C Bus (Addr:`0x27`) | Màn hình hiển thị 4 dòng tại trạm bờ ao             |
| **Relay 1**               |           GPIO 19           |      Digital Output      | Điều khiển đóng/cắt Quạt nước chính               |
| **Relay 2**               |           GPIO 25           |      Digital Output      | Điều khiển đóng/cắt Quạt nước tăng cường        |
| **Servo SG90**            |           GPIO 26           |        PWM Output        | Cơ cấu đóng/mở van xả thức ăn tự động            |
| **Còi Buzzer**           |           GPIO 27           |      Digital Output      | Cảnh báo âm thanh sự cố tại bờ ao                    |
| **LED Xanh**              |           GPIO 16           |      Digital Output      | Báo trạng thái hệ thống An toàn                       |
| **LED Đỏ**              |           GPIO 14           |      Digital Output      | Báo trạng thái sự cố nguy hiểm                        |

---

## 7. CÂY TOPIC MQTT CLOUD

Hệ thống giao tiếp qua máy chủ công cộng: `broker.hivemq.com`

| Tên Topic                 | Hướng truyền |    QoS    | Chức năng                                                                                                 |
| :------------------------- | :--------------: | :--------: | :---------------------------------------------------------------------------------------------------------- |
| `nhom2_quanhoicanh/farm/pond_01/status`    | ESP32$\to$ Web | 1 (Retain) | Cơ chế LWT báo Online khi hoạt động, tự động báo Offline khi trạm mất điện/đứt mạng        |
| `nhom2_quanhoicanh/farm/pond_01/telemetry` | ESP32$\to$ Web |     0     | Xuất bản gói tin JSON định kỳ mỗi 2 giây chứa đầy đủ 6 thông số và trạng thái chấp hành |
| `nhom2_quanhoicanh/farm/pond_01/alert`     | ESP32$\to$ Web |     1     | Phát cảnh báo khẩn cấp tức thời khi thông số vượt ngưỡng an toàn                              |
| `nhom2_quanhoicanh/farm/pond_01/control`   | Web$\to$ ESP32 |     1     | Gửi lệnh điều khiển bật/tắt Quạt và xả thức ăn thủ công                                       |
| `nhom2_quanhoicanh/farm/pond_01/sandbox`   | Web$\to$ ESP32 |     0     | Gửi gói tin giả lập tham số từ thanh trượt Digital Twin xuống vi điều khiển                     |

---

## 8. XỬ LÝ SỰ CỐ THƯỜNG GẶP (TROUBLESHOOTING)

1. **Web báo `MQTT: MẤT KẾT NỐI`:**
   - Kiểm tra đường truyền Internet của máy tính.
   - Broker HiveMQ sử dụng cổng WebSocket Secure `8884` (`wss://`). Đảm bảo tường lửa hoặc mạng Wi-Fi không chặn cổng 8884.
2. **Web báo `TRẠM AO: CHỜ TÍN HIỆU (LWT)`:**
   - Do mô phỏng Wokwi chưa được bật. Hãy mở file `diagram.json` và nhấn nút Play màu xanh để khởi động ESP32.
3. **Wokwi báo lỗi thiếu file `.bin`:**
   - Nhấn icon **dấu tích (Build)** của PlatformIO để biên dịch lại mã nguồn C++. Khi build thành công, file `.pio/build/az-delivery-devkit-v4/firmware.bin` sẽ được tạo mới.
4. **Trình duyệt không phát tiếng chuông khi có sự cố:**
   - Một số trình duyệt (như Chrome) chặn âm thanh tự động phát nếu người dùng chưa tương tác với trang. Hãy click chuột một lần vào bất kỳ vị trí nào trên trang web để cấp quyền âm thanh cho Web Audio API.
