const firebaseConfig = {
  apiKey: "AIzaSyDeb_ObwbK0yVsmWncYoSCbxjGXFGPMbD4",
  authDomain: "btl-he-nhung-ngoquanganh.firebaseapp.com",
  databaseURL: "https://btl-he-nhung-ngoquanganh-default-rtdb.asia-southeast1.firebasedatabase.app",
  projectId: "btl-he-nhung-ngoquanganh",
  storageBucket: "btl-he-nhung-ngoquanganh.firebasestorage.app",
  messagingSenderId: "33391037967",
  appId: "1:33391037967:web:fbce8149b330ce2c7edd7a",
  measurementId: "G-EKQ8C6CV82"
};
// Initialize Firebase
firebase.initializeApp(firebaseConfig);
const database = firebase.database();

// References to DOM elements
const temperatureElement = document.getElementById("temperature");
const humidityElement = document.getElementById("humidity");
const rainMessageElement = document.getElementById("rainMessage");
const servoPositionElement = document.getElementById("servoPosition");
const modeStatusElement = document.getElementById("modeStatus");
const currentTimeElement = document.getElementById("currentTime");
const schedulePhoiElement = document.getElementById("schedulePhoi");
const scheduleThuElement = document.getElementById("scheduleThu");
const phoiTimeInput = document.getElementById("phoiTime");
const thuTimeInput = document.getElementById("thuTime");

// Listen for changes in sensor data
database.ref("/sensor").on("value", (snapshot) => {
    const data = snapshot.val();
    if (data) {
        temperatureElement.textContent = data.temperature !== undefined ? data.temperature : "N/A";
        humidityElement.textContent = data.humidity !== undefined ? data.humidity : "N/A";

        if (data.rain) {
            rainMessageElement.textContent = "Trời đang mưa!";
            rainMessageElement.className = "status raining";
        } else {
            rainMessageElement.textContent = "Trời không mưa!";
            rainMessageElement.className = "status not-raining";
        }
    } else {
        console.error("Không đọc được dữ liệu từ /sensor");
    }
}, (error) => {
    console.error("Lỗi khi đọc dữ liệu từ /sensor:", error);
});

// Listen for changes in servo position
database.ref("/servo/position").on("value", (snapshot) => {
    const position = snapshot.val();
    servoPositionElement.textContent = position !== null ? position : "N/A";
}, (error) => {
    console.error("Lỗi khi đọc dữ liệu từ /servo/position:", error);
});

// Listen for changes in manual mode
database.ref("/servo/manual").on("value", (snapshot) => {
    const isManual = snapshot.val();
    if (isManual !== null) {
        modeStatusElement.textContent = `Chế độ: ${isManual ? "Thủ công" : "Tự động"}`;
        modeStatusElement.className = `mode ${isManual ? "mode-manual" : "mode-auto"}`;
    } else {
        modeStatusElement.textContent = "Chế độ: N/A";
    }
}, (error) => {
    console.error("Lỗi khi đọc dữ liệu từ /servo/manual:", error);
});

// Listen for changes in schedule
database.ref("/schedule/phoi").on("value", (snapshot) => {
    const phoiTime = snapshot.val();
    schedulePhoiElement.textContent = phoiTime || "6:00";
}, (error) => {
    console.error("Lỗi khi đọc dữ liệu từ /schedule/phoi:", error);
});

database.ref("/schedule/thu").on("value", (snapshot) => {
    const thuTime = snapshot.val();
    scheduleThuElement.textContent = thuTime || "18:00";
}, (error) => {
    console.error("Lỗi khi đọc dữ liệu từ /schedule/thu:", error);
});

// Listen for changes in current time from Firebase
database.ref("/time/current").on("value", (snapshot) => {
    const currentTime = snapshot.val();
    currentTimeElement.textContent = currentTime || "N/A";
}, (error) => {
    console.error("Lỗi khi đọc thời gian từ /time/current:", error);
});

// Manual servo control using Firebase
function moveServo(position) {
    Promise.all([
        database.ref("/servo/position").set(position),
        database.ref("/servo/manual").set(true)
    ])
    .then(() => {
        console.log(`Cập nhật vị trí servo trên Firebase: ${position}° (chế độ thủ công)`);
    })
    .catch(error => {
        console.error("Lỗi khi cập nhật vị trí servo trên Firebase:", error);
        alert(`Không thể điều khiển servo. Lỗi: ${error.message}. Vui lòng kiểm tra kết nối Firebase.`);
    });
}

// Chuyển về chế độ tự động
function setAutoMode() {
    database.ref("/servo/manual").set(false)
        .then(() => {
            console.log("Chuyển về chế độ tự động");
        })
        .catch(error => {
            console.error("Lỗi khi chuyển về chế độ tự động:", error);
            alert(`Không thể chuyển về chế độ tự động. Lỗi: ${error.message}. Vui lòng kiểm tra kết nối Firebase.`);
        });
}

// Lưu lịch hẹn giờ
function saveSchedule() {
    const phoiTime = phoiTimeInput.value;
    const thuTime = thuTimeInput.value;

    if (phoiTime && thuTime) {
        Promise.all([
            database.ref("/schedule/phoi").set(phoiTime),
            database.ref("/schedule/thu").set(thuTime)
        ])
        .then(() => {
            console.log(`Lưu lịch hẹn giờ: Phơi: ${phoiTime}, Thu: ${thuTime}`);
            alert("Lưu lịch hẹn giờ thành công!");
        })
        .catch(error => {
            console.error("Lỗi khi lưu lịch hẹn giờ:", error);
            alert(`Không thể lưu lịch hẹn giờ. Lỗi: ${error.message}. Vui lòng kiểm tra kết nối Firebase.`);
        });
    } else {
        alert("Vui lòng nhập đầy đủ thời gian phơi và thu!");
    }
}