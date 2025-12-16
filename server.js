require("dotenv").config();
const express = require("express");
const mongoose = require("mongoose");
const mqtt = require("mqtt");
const http = require("http");
const { Server } = require("socket.io");
const path = require("path");

// --- 1. SETUP SERVER ---
const app = express();
const server = http.createServer(app);
const io = new Server(server);

app.use(express.static(path.join(__dirname, "public")));
app.use(express.json());

// --- 2. KẾT NỐI MONGODB ---
mongoose
  .connect(process.env.MONGO_URI)
  .then(() => console.log("✅ Đã kết nối MongoDB Cloud"))
  .catch((err) => console.error("❌ Lỗi MongoDB:", err));

// Định nghĩa cấu trúc dữ liệu (Schema)
const LogSchema = new mongoose.Schema({
  timestamp: { type: Date, default: Date.now },
  toc_do: Number,
  khoang_cach: Number,
  trang_thai: String,
});
const LogModel = mongoose.model("Log", LogSchema);

// --- 3. KẾT NỐI MQTT (HIVEMQ) ---
const mqttClient = mqtt.connect(process.env.MQTT_HOST, {
  username: process.env.MQTT_USER,
  password: process.env.MQTT_PASS,
  port: process.env.MQTT_PORT,
  protocol: "mqtts", // Bắt buộc SSL
});

mqttClient.on("connect", () => {
  console.log("✅ Đã kết nối HiveMQ Cloud");
  mqttClient.subscribe("robot/data");
});

// Khi nhận dữ liệu từ xe -> Lưu DB & Gửi ra Web
mqttClient.on("message", async (topic, message) => {
  try {
    const data = JSON.parse(message.toString());

    // 1. Lưu vào MongoDB
    const newLog = new LogModel(data);
    await newLog.save();

    // 2. Bắn tin hiệu ra Dashboard (Realtime)
    io.emit("sensor_update", data);
  } catch (e) {
    console.error("Lỗi dữ liệu rác:", e);
  }
});

// --- 4. GIAO TIẾP VỚI DASHBOARD (SOCKET.IO) ---
io.on("connection", (socket) => {
  console.log("Client Web đã kết nối");

  // Khi người dùng bấm nút trên Web -> Gửi lệnh xuống MQTT
  socket.on("control_cmd", (cmd) => {
    mqttClient.publish("robot/control", cmd);
    console.log("Gửi lệnh:", cmd);
  });

  socket.on("speed_cmd", (val) => {
    mqttClient.publish("robot/speed", val.toString());
  });

  // Gửi 10 dòng lịch sử gần nhất khi mới vào web (Giữ nguyên)
  LogModel.find()
    .sort({ timestamp: -1 })
    .limit(10)
    .then((logs) => socket.emit("history_logs", logs));

  // --- [MỚI] XỬ LÝ YÊU CẦU LẤY LỊCH SỬ TỪ WEB (Đã tối ưu) ---
  socket.on("get_history", async (data) => {
    console.log("Web yêu cầu lịch sử:", data);
    try {
      // Chuyển đổi chuỗi thời gian từ client thành đối tượng Date
      const start = new Date(data.startTime);
      const end = new Date(data.endTime);

      // Truy vấn MongoDB với giới hạn (LIMIT)
      const logs = await LogModel.find({
        timestamp: {
          $gte: start, // Lớn hơn hoặc bằng thời gian bắt đầu
          $lte: end, // Nhỏ hơn hoặc bằng thời gian kết thúc
        },
      })
        .sort({ timestamp: -1 }) // Sắp xếp mới nhất lên đầu
        .limit(1000); // QUAN TRỌNG: Chỉ lấy tối đa 1000 dòng để không treo máy

      // Gửi kết quả ngược lại cho Web Client
      socket.emit("history_data_result", logs);
      console.log(`Đã gửi ${logs.length} bản ghi lịch sử.`);
    } catch (err) {
      console.error("Lỗi truy vấn lịch sử:", err);
      socket.emit("history_data_result", []); // Gửi mảng rỗng nếu lỗi
    }
  });
});

// --- 5. CHẠY SERVER ---
const PORT = process.env.PORT || 3000;
server.listen(PORT, () => console.log(`🚀 Server đang chạy tại port ${PORT}`));
