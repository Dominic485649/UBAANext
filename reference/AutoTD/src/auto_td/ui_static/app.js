const notice = document.querySelector("#notice");
const usersBody = document.querySelector("#users-body");
const imageGrid = document.querySelector("#image-grid");
const backgroundButton = document.querySelector("#background-button");
const runOnceButton = document.querySelector("#run-once-button");
const refreshButton = document.querySelector("#refresh-button");
const addUserForm = document.querySelector("#add-user-form");
const imageForm = document.querySelector("#image-form");

let currentState = null;

function setNotice(message, type = "") {
  notice.textContent = message;
  notice.className = `notice ${type}`.trim();
}

async function requestJson(url, options = {}) {
  const response = await fetch(url, {
    ...options,
    headers: options.body instanceof FormData
      ? options.headers || {}
      : { "Content-Type": "application/json", ...(options.headers || {}) },
  });
  const payload = await response.json().catch(() => ({}));
  if (!response.ok) {
    throw new Error(payload.error || `请求失败：${response.status}`);
  }
  return payload;
}

function badgeClass(status) {
  if (status === "completed") return "badge completed";
  if (status === "waiting") return "badge waiting";
  if (status === "error") return "badge error";
  return "badge";
}

function statusLabel(status) {
  const labels = {
    pending: "待处理",
    waiting: "等待中",
    completed: "已完成",
    error: "错误",
  };
  return labels[status] || status || "-";
}

function actionLabel(action) {
  const labels = {
    entrance: "入口",
    exit: "出口",
  };
  return labels[action] || action || "-";
}

function renderUsers(users) {
  usersBody.textContent = "";
  if (!users.length) {
    const row = document.createElement("tr");
    const cell = document.createElement("td");
    cell.colSpan = 6;
    cell.className = "empty-row";
    cell.textContent = "暂无用户";
    row.append(cell);
    usersBody.append(row);
    return;
  }

  for (const user of users) {
    const row = document.createElement("tr");
    const progress = `${user.today.completed_rounds}/${user.today.rounds}`;
    const fields = [
      user.student_id,
      progress,
      user.td_count ?? "未缓存",
      actionLabel(user.today.next_action),
      user.today.message || "-",
    ];
    fields.forEach((value, index) => {
      const cell = document.createElement("td");
      if (index === 1) {
        const badge = document.createElement("span");
        badge.className = badgeClass(user.today.status);
        badge.textContent = `${statusLabel(user.today.status)} ${value}`;
        cell.append(badge);
      } else {
        cell.textContent = value;
      }
      row.append(cell);
    });

    const actionCell = document.createElement("td");
    const button = document.createElement("button");
    button.type = "button";
    button.className = "danger";
    button.textContent = "删除";
    button.addEventListener("click", () => deleteUser(user.student_id));
    actionCell.append(button);
    row.append(actionCell);
    usersBody.append(row);
  }
}

function renderImages(images) {
  imageGrid.textContent = "";
  if (!images.length) {
    const empty = document.createElement("p");
    empty.className = "empty-row";
    empty.textContent = "暂无图片";
    imageGrid.append(empty);
    return;
  }

  for (const image of images) {
    const item = document.createElement("article");
    item.className = "image-item";
    const preview = document.createElement("img");
    preview.src = `${image.url}?t=${encodeURIComponent(image.updated_at)}`;
    preview.alt = `图片预览：${image.name}`;
    const meta = document.createElement("div");
    meta.className = "image-meta";
    meta.textContent = `${image.name} · ${Math.ceil(image.size / 1024)} KB`;
    const button = document.createElement("button");
    button.type = "button";
    button.className = "danger";
    button.textContent = "删除图片";
    button.addEventListener("click", () => deleteImage(image.name));
    item.append(preview, meta, button);
    imageGrid.append(item);
  }
}

function renderState(state) {
  currentState = state;
  document.querySelector("#background-status").textContent = state.background.message;
  document.querySelector("#home-path").textContent = state.home;
  document.querySelector("#refreshed-at").textContent = state.refreshed_at;
  backgroundButton.textContent = state.background.running
    ? `停止后台 PID ${state.background.pid}`
    : "运行后台";
  backgroundButton.classList.toggle("danger", state.background.running);
  backgroundButton.setAttribute(
    "aria-label",
    state.background.running ? `停止后台进程 ${state.background.pid}` : "运行后台打卡进程",
  );
  renderUsers(state.users);
  renderImages(state.images);
}

async function refresh(message = "状态已刷新") {
  const state = await requestJson("/api/state");
  renderState(state);
  setNotice(message, "success");
}

async function withBusy(button, label, operation) {
  const previous = button.textContent;
  button.disabled = true;
  button.textContent = label;
  try {
    await operation();
  } catch (error) {
    setNotice(error.message, "error");
  } finally {
    button.disabled = false;
    button.textContent = previous;
    if (currentState) {
      renderState(currentState);
    }
  }
}

async function deleteUser(studentId) {
  await withBusy(refreshButton, "处理中", async () => {
    await requestJson(`/api/users/${encodeURIComponent(studentId)}`, { method: "DELETE" });
    await refresh(`已删除用户 ${studentId}`);
  });
}

async function deleteImage(name) {
  await withBusy(refreshButton, "处理中", async () => {
    await requestJson(`/api/images/${encodeURIComponent(name)}`, { method: "DELETE" });
    await refresh(`已删除图片 ${name}`);
  });
}

refreshButton.addEventListener("click", () => {
  withBusy(refreshButton, "刷新中", () => refresh("状态已刷新"));
});

backgroundButton.addEventListener("click", () => {
  withBusy(backgroundButton, "处理中", async () => {
    const result = await requestJson("/api/background/toggle", { method: "POST", body: "{}" });
    await refresh(result.message || "后台状态已更新");
  });
});

runOnceButton.addEventListener("click", () => {
  withBusy(runOnceButton, "打卡中", async () => {
    const result = await requestJson("/api/run-once", { method: "POST", body: "{}" });
    await refresh(`今日打卡完成：成功 ${result.success}，失败 ${result.failure}`);
  });
});

addUserForm.addEventListener("submit", (event) => {
  event.preventDefault();
  const formData = new FormData(addUserForm);
  withBusy(addUserForm.querySelector("button"), "新增中", async () => {
    const studentId = String(formData.get("student_id") || "").trim();
    const campus = String(formData.get("campus") || "").trim();
    await requestJson("/api/users", {
      method: "POST",
      body: JSON.stringify({ student_id: studentId, campus }),
    });
    addUserForm.reset();
    document.querySelector("#campus").value = "学院路";
    await refresh(`已新增用户 ${studentId}`);
  });
});

imageForm.addEventListener("submit", (event) => {
  event.preventDefault();
  const formData = new FormData();
  const imageInput = document.querySelector("#image-input");
  const file = imageInput.files[0];
  if (!file) {
    setNotice("请选择要上传的图片", "error");
    return;
  }
  formData.append("image", file);
  if (document.querySelector("#overwrite-image").checked) {
    formData.append("overwrite", "true");
  }
  withBusy(imageForm.querySelector("button"), "上传中", async () => {
    await requestJson("/api/images", { method: "POST", body: formData });
    imageForm.reset();
    await refresh(`已上传图片 ${file.name}`);
  });
});

refresh("状态已刷新").catch((error) => setNotice(error.message, "error"));
