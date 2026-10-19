/**
 * =============================================================================
 * FILE MẪU CẤU HÌNH WEB SCADA DASHBOARD (CONFIG TEMPLATE)
 * =============================================================================
 * HƯỚNG DẪN:
 * 1. Sao chép file này thành 'config.js' trong cùng thư mục web/:
 *    (Ví dụ: copy config.example.js config.js)
 * 2. Cập nhật thông tin máy chủ EMQX Cloud và tài khoản của bạn.
 * =============================================================================
 */
window.ENV_CONFIG = {
  // Máy chủ EMQX Cloud Private (Cổng WebSocket Secure 8084)
  MQTT_HOST: "your_cluster_id.ala.asia-southeast1.emqxsl.com",
  MQTT_PORT: 8084,
  MQTT_PATH: "/mqtt",
  MQTT_USER: "your_mqtt_username",
  MQTT_PASSWORD: "your_mqtt_password",

  // Tiền tố Topic chuẩn hóa nhóm
  TOPIC_PREFIX: "nhom2_quanhoicanh/farm/pond_01"
};
