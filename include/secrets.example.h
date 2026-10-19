#ifndef SECRETS_EXAMPLE_H
#define SECRETS_EXAMPLE_H

// =============================================================================
// FILE MẪU CẤU HÌNH BẢO MẬT ESP32 FIRMWARE (SECRETS TEMPLATE)
// =============================================================================
// HƯỚNG DẪN:
// 1. Sao chép file này thành file 'secrets.h' trong cùng thư mục include/
//    (Ví dụ: copy secrets.example.h secrets.h)
// 2. Điền thông tin Broker, Cổng TLS, User và Password thực tế của cụm EMQX Cloud.
// =============================================================================

// Cấu hình Wi-Fi
#define SECRET_WIFI_SSID        "Wokwi-GUEST"
#define SECRET_WIFI_PASSWORD    ""

// Cấu hình MQTT Broker (EMQX Cloud Serverless - TLS Port 8883)
#define SECRET_MQTT_BROKER      "your_cluster_id.ala.asia-southeast1.emqxsl.com"
#define SECRET_MQTT_PORT        8883
#define SECRET_MQTT_USER        "your_mqtt_username"
#define SECRET_MQTT_PASSWORD    "your_mqtt_password"

// Tiền tố Topic chuẩn hóa định danh nhóm
#define SECRET_TOPIC_PREFIX     "nhom2_quanhoicanh/farm/pond_01"

#endif // SECRETS_EXAMPLE_H
