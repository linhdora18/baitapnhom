
const brands = ["CHANEL", "YSL", "DIOR", "GUCCI", "HERMÈS"];

const slots = brands.map((brand, index) => ({
  id: index + 1,
  brand,
  remaining: 100,
  threshold: 15,
  enabled: true,
  sensorError: false,
  pumpError: false,
  active: false,
  lastError: "Không có"
}));

let orders = 0;
let emergencyStopped = false;
let logs = [];

const $ = (id) => document.getElementById(id);

function addLog(message) {
  const time = new Date().toLocaleTimeString("vi-VN");
  logs.unshift(`[${time}] ${message}`);
  renderLogs();
}

function renderLogs() {
  const area = $("logs");
  area.replaceChildren();

  logs.forEach((message) => {
    const line = document.createElement("div");
    line.className = "log-line";
    line.textContent = message;
    area.appendChild(line);
  });
}

function buildSlots() {
  const container = $("slots");
  container.replaceChildren();

  slots.forEach((slot) => {
    const card = document.createElement("article");
    card.className = "slot";

    card.innerHTML = `
      <h3>
        Slot ${slot.id}
        <span id="status-${slot.id}">READY</span>
      </h3>
      <div class="brand">${slot.brand}</div>

      <div class="bar">
        <div class="fill" id="bar-${slot.id}"></div>
      </div>

      <p class="stock" id="stock-${slot.id}"></p>

      <input
        type="range"
        min="0"
        max="100"
        value="${slot.remaining}"
        id="range-${slot.id}"
        aria-label="Mức tồn slot ${slot.id}"
      >

      <p class="slot-info">
        Ngưỡng tồn thấp:
        <input
          type="number"
          min="0"
          max="100"
          value="${slot.threshold}"
          id="threshold-${slot.id}"
          style="width:55px"
        >
      </p>

      <p class="slot-info" id="error-${slot.id}">
        Lỗi gần nhất: Không có
      </p>

      <label>
        <input type="checkbox" id="enabled-${slot.id}" checked>
        Cho phép hoạt động
      </label>

      <label>
        <input type="checkbox" id="sensor-${slot.id}">
        Lỗi cảm biến
      </label>

      <label>
        <input type="checkbox" id="pump-${slot.id}">
        Bơm không ra
      </label>

      <button id="spray-${slot.id}">
        Phun thử (chỉ mô phỏng)
      </button>

      <button id="refill-${slot.id}">
        Nạp lại 100 lượt
      </button>
    `;

    container.appendChild(card);

    $(`range-${slot.id}`).addEventListener("input", (event) => {
      slot.remaining = Number(event.target.value);
      render();
    });

    $(`threshold-${slot.id}`).addEventListener("change", (event) => {
      slot.threshold = Math.max(
        0,
        Math.min(100, Number(event.target.value) || 0)
      );
      event.target.value = slot.threshold;
      render();
    });

    $(`enabled-${slot.id}`).addEventListener("change", (event) => {
      slot.enabled = event.target.checked;
      addLog(`Slot ${slot.id}: ${slot.enabled ? "đã bật" : "đã tắt"}.`);
      render();
    });

    $(`sensor-${slot.id}`).addEventListener("change", (event) => {
      slot.sensorError = event.target.checked;
      slot.lastError = slot.sensorError ? "SENSOR_FAILURE" : "Không có";
      addLog(`Slot ${slot.id}: ${slot.sensorError
        ? "mô phỏng lỗi cảm biến"
        : "đã xóa lỗi cảm biến"}.`);
      render();
    });

    $(`pump-${slot.id}`).addEventListener("change", (event) => {
      slot.pumpError = event.target.checked;
      slot.lastError = slot.pumpError ? "NO_FLOW" : "Không có";
      addLog(`Slot ${slot.id}: ${slot.pumpError
        ? "mô phỏng lỗi bơm"
        : "đã xóa lỗi bơm"}.`);
      render();
    });

    $(`spray-${slot.id}`).addEventListener("click", () => {
      spraySlot(slot.id);
    });

    $(`refill-${slot.id}`).addEventListener("click", async () => {
    try {
        const response = await fetch("/api/refill", {
            method: "POST",
            headers: {
                "Content-Type": "application/json"
            },
            body: JSON.stringify({ slot: slot.id })
        });

        const data = await response.json();

        if (!response.ok || !data.ok) {
            throw new Error(data.error || "REFILL_FAILED");
        }

        slot.remaining = data.remaining;
        slot.sensorError = false;
        slot.pumpError = false;
        slot.lastError = "Không có";

        $(`sensor-${slot.id}`).checked = false;
        $(`pump-${slot.id}`).checked = false;

        addLog(`Slot ${slot.id}: đã nạp lại 100 lượt.`);
        render();
      } catch (error) {
        addLog(`Slot ${slot.id}: nạp lại thất bại - ${error.message}`);
    }
  }); // Đóng sự kiện nạp lại

  }); // Đóng vòng lặp 5 slot
} // Đóng hàm buildSlots()
async function sendSprayToBackend(id) {
    const response = await fetch("/api/spray", {
        method: "POST",
        headers: {
            "Content-Type": "application/json"
        },
        body: JSON.stringify({ slot: id })
    });

    const data = await response.json();

    if (!response.ok || !data.ok) {
        throw new Error(data.error || "SPRAY_FAILED");
    }

        return data;
}
async function spraySlot(id) {
    const slot = slots.find((item) => item.id === id);

    if (!slot) {
        addLog(`TỪ CHỐI: Slot ${id} không tồn tại.`);
        return;
    }

    if (emergencyStopped) {
        addLog("TỪ CHỐI: E-STOP đang bật.");
        return;
    }

    if ($("door").checked) {
        addLog(`TỪ CHỐI Slot ${id}: cửa mở / interlock.`);
        return;
    }

    if ($("critical").checked) {
        addLog(`TỪ CHỐI Slot ${id}: lỗi nghiêm trọng.`);
        return;
    }

    if ($("maintenance").checked) {
        addLog(`TỪ CHỐI Slot ${id}: máy đang bảo trì.`);
        return;
    }

    if ($("offline").checked) {
        addLog(`TỪ CHỐI Slot ${id}: mất kết nối.`);
        return;
    }

    if (!slot.enabled) {
        addLog(`TỪ CHỐI Slot ${id}: slot đã tắt.`);
        return;
    }

    if (slot.sensorError) {
        addLog(`TỪ CHỐI Slot ${id}: SENSOR_FAILURE.`);
        return;
    }

    if (slot.pumpError) {
        slot.lastError = "NO_FLOW";
        addLog(`LỖI Slot ${id}: NO_FLOW.`);
        render();
        return;
    }

    if (slot.remaining <= 0) {
        addLog(`TỪ CHỐI Slot ${id}: hết tồn kho.`);
        return;
    }

    if (slot.active) {
        addLog(`Slot ${id}: đang xử lý phun.`);
        return;
    }

    try {
        slot.active = true;
        render();

        const result = await sendSprayToBackend(id);

        slot.remaining = result.remaining;
        slot.lastError = "Không có";
        orders += 1;

        addLog(`Slot ${id}: phun thử mô phỏng thành công.`);
        render();

        setTimeout(() => {
            slot.active = false;
            render();
        }, result.duration_ms || 700);

    } catch (error) {
        slot.active = false;
        slot.lastError = error.message;

        addLog(
            `Slot ${id}: Backend từ chối - ${error.message}`
        );

        render();
    }
}
function render() {
  $("enabledCount").textContent =
    `${slots.filter((slot) => slot.enabled).length}/5`;

  $("totalRemaining").textContent =
    slots.reduce((sum, slot) => sum + slot.remaining, 0);

  $("lowCount").textContent =
    slots.filter((slot) => slot.remaining <= slot.threshold).length;

  $("orders").textContent = orders;
  $("connection").textContent = $("offline").checked
    ? "OFFLINE"
    : "ONLINE";
  $("doorState").textContent = $("door").checked ? "mở" : "đóng";

  let machineState = "IDLE";

  if (emergencyStopped) {
    machineState = "E-STOP";
  } else if ($("critical").checked) {
    machineState = "FAULT";
  } else if ($("maintenance").checked) {
    machineState = "MAINTENANCE";
  } else if (slots.some((slot) => slot.active)) {
    machineState = "DISPENSING";
  }

  $("machineState").textContent = machineState;
  $("machineState").style.background =
    ["E-STOP", "FAULT"].includes(machineState) ? "#ff5867" : "#2fc582";

  if (emergencyStopped) {
    $("safetyMessage").textContent =
      "E-STOP đang bật: thao tác phun bị khóa trong mô phỏng.";
  } else if ($("door").checked) {
    $("safetyMessage").textContent =
      "Cửa đang mở: interlock chặn thao tác phun mô phỏng.";
  } else if ($("critical").checked) {
    $("safetyMessage").textContent =
      "Có lỗi nghiêm trọng: thao tác phun bị khóa trong mô phỏng.";
  } else if ($("maintenance").checked) {
    $("safetyMessage").textContent =
      "Máy đang ở chế độ bảo trì.";
  } else if ($("offline").checked) {
    $("safetyMessage").textContent =
      "Mất kết nối mô phỏng: thao tác phun bị chặn.";
  } else {
    $("safetyMessage").textContent =
      "Giao diện mô phỏng đang sẵn sàng.";
  }

  slots.forEach((slot) => {
    $(`bar-${slot.id}`).style.width = `${slot.remaining}%`;
    $(`stock-${slot.id}`).textContent =
      `Còn ước tính ${slot.remaining} lượt xịt`;

    let status = "READY";

    if (slot.active) status = "DISPENSING";
    else if (!slot.enabled) status = "DISABLED";
    else if (slot.sensorError) status = "SENSOR ERROR";
    else if (slot.pumpError) status = "PUMP ERROR";
    else if (slot.remaining <= slot.threshold) status = "LOW STOCK";

    $(`status-${slot.id}`).textContent = status;
    $(`error-${slot.id}`).textContent =
      `Lỗi gần nhất: ${slot.lastError}`;
  });
}

["door", "critical", "maintenance", "offline"].forEach((id) => {
  $(id).addEventListener("change", () => {
    addLog(`${id}: ${$(id).checked ? "bật" : "tắt"}.`);
    render();
  });
});

$("estop").addEventListener("click", () => {
  emergencyStopped = true;
  slots.forEach((slot) => {
    slot.active = false;
  });

  addLog("E-STOP: khóa mọi thao tác phun mô phỏng.");
  render();
});

$("reset").addEventListener("click", () => {
  emergencyStopped = false;
  slots.forEach((slot) => {
    slot.active = false;
  });

  addLog("Reset mô phỏng. Hãy kiểm tra các điều kiện an toàn.");
  render();
});

$("clearLog").addEventListener("click", () => {
  logs = [];
  renderLogs();
});

$("exportLog").addEventListener("click", () => {
  const blob = new Blob(
    [logs.slice().reverse().join("\n") + "\n"],
    { type: "text/plain;charset=utf-8" }
  );

  const url = URL.createObjectURL(blob);
  const link = document.createElement("a");
  link.href = url;
  link.download = "scentstation-log.txt";
  link.click();
  URL.revokeObjectURL(url);
});

buildSlots();
render();
addLog("Khởi động prototype mô phỏng 5 slot.");

async function checkBackend() {
  try {
    const response = await fetch("/api/status");

    if (!response.ok) {
      throw new Error("HTTP " + response.status);
    }

    const data = await response.json();

    console.log("Backend ScentStation:", data);
    addLog("Đã kết nối backend Python.");
  } catch (error) {
    console.error("Lỗi kết nối backend:", error);
    addLog("Không kết nối được backend Python.");
  }
}

checkBackend();

