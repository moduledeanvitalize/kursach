const STATUS = {
  0: "К выполнению",
  1: "В работе",
  2: "На проверке",
  3: "Готово"
};

const state = {
  projects: [],
  currentProjectId: null,
  projectEditMode: false,
  editingTaskId: null,
  tasks: [],
  gantt: { tasks: [], critical_path: [], bottlenecks: [] },
  analytics: null,
  commands: [],
  notifications: [],
  chart: null
};

const refreshBtn = document.getElementById("refreshBtn");
const projectSelect = document.getElementById("projectSelect");
const newProjectBtn = document.getElementById("newProjectBtn");
const editProjectBtn = document.getElementById("editProjectBtn");
const deleteProjectBtn = document.getElementById("deleteProjectBtn");
const projectForm = document.getElementById("projectForm");
const projectSubmitBtn = document.getElementById("projectSubmitBtn");
const projectCancelEditBtn = document.getElementById("projectCancelEditBtn");
const taskForm = document.getElementById("taskForm");
const taskSubmitBtn = document.getElementById("taskSubmitBtn");
const taskCancelEditBtn = document.getElementById("taskCancelEditBtn");
const assigneeFilter = document.getElementById("assigneeFilter");
const kanbanBoard = document.getElementById("kanbanBoard");
const ganttCanvas = document.getElementById("ganttCanvas");
const commandsList = document.getElementById("commandsList");
const notificationsList = document.getElementById("notificationsList");

function toTs(dateStr) {
  if (!dateStr) return 0;
  const date = new Date(`${dateStr}T00:00:00`);
  return Math.floor(date.getTime() / 1000);
}

function fromTs(ts) {
  if (!ts) return "";
  const date = new Date(ts * 1000);
  return date.toISOString().slice(0, 10);
}

function fmtDate(ts) {
  if (!ts) return "-";
  return new Date(ts * 1000).toLocaleDateString("ru-RU");
}

async function api(url, options = {}) {
  const response = await fetch(url, {
    headers: { "Content-Type": "application/json" },
    ...options
  });

  if (!response.ok) {
    const text = await response.text();
    let errorMessage = text || `HTTP ${response.status}`;
    try {
      const parsed = JSON.parse(text);
      if (parsed && typeof parsed.error === "string" && parsed.error.trim()) {
        errorMessage = parsed.error;
      }
    } catch {
      // Non-JSON response, fall back to raw text.
    }
    throw new Error(errorMessage);
  }

  return response.json();
}

function setProjectFormMode(isEdit) {
  state.projectEditMode = isEdit;
  if (isEdit) {
    projectSubmitBtn.textContent = "Сохранить изменения проекта";
    projectCancelEditBtn.classList.remove("hidden");
    return;
  }
  projectSubmitBtn.textContent = "Создать проект";
  projectCancelEditBtn.classList.add("hidden");
}

function resetProjectFormForCreate() {
  const now = Math.floor(Date.now() / 1000);
  document.getElementById("projectName").value = "";
  document.getElementById("projectDescription").value = "";
  document.getElementById("projectStart").value = fromTs(now);
  document.getElementById("projectEnd").value = fromTs(now + 7 * 86400);
  setProjectFormMode(false);
}

function setTaskFormMode(isEdit) {
  if (isEdit) {
    taskSubmitBtn.textContent = "Сохранить задачу";
    taskCancelEditBtn.classList.remove("hidden");
    return;
  }
  taskSubmitBtn.textContent = "Создать задачу";
  taskCancelEditBtn.classList.add("hidden");
}

function resetTaskForm() {
  state.editingTaskId = null;
  taskForm.reset();
  document.getElementById("taskProgress").value = "0";
  setTaskFormMode(false);
}

function renderProjects() {
  projectSelect.innerHTML = "";

  if (!state.projects.length) {
    const option = document.createElement("option");
    option.value = "";
    option.textContent = "Проектов нет";
    projectSelect.appendChild(option);
    return;
  }

  state.projects.forEach((project) => {
    const option = document.createElement("option");
    option.value = String(project.id);
    option.textContent = `${project.id}: ${project.name}`;
    if (project.id === state.currentProjectId) {
      option.selected = true;
    }
    projectSelect.appendChild(option);
  });

  const current = state.projects.find((item) => item.id === state.currentProjectId);
  if (!current) return;

  document.getElementById("projectName").value = current.name || "";
  document.getElementById("projectDescription").value = current.description || "";
  document.getElementById("projectStart").value = fromTs(current.start_date);
  document.getElementById("projectEnd").value = fromTs(current.end_date);
}

function startTaskEdit(taskId) {
  const task = state.tasks.find((item) => item.id === taskId);
  if (!task) return;

  state.editingTaskId = task.id;
  document.getElementById("taskTitle").value = task.title || "";
  document.getElementById("taskDescription").value = task.description || "";
  document.getElementById("taskAssignee").value = task.assignee || "";
  document.getElementById("taskStatus").value = String(task.status);
  document.getElementById("taskStart").value = fromTs(task.start_date);
  document.getElementById("taskEnd").value = fromTs(task.end_date);
  document.getElementById("taskProgress").value = String(task.progress || 0);
  document.getElementById("taskDependency").value = task.dependencies && task.dependencies.length ? String(task.dependencies[0]) : "";
  setTaskFormMode(true);
  document.getElementById("taskTitle").focus();
}

function renderAssigneeFilter() {
  const current = assigneeFilter.value;
  const assignees = Array.from(new Set(state.tasks.map((task) => (task.assignee || "").trim()).filter(Boolean))).sort((a, b) => a.localeCompare(b, "ru"));

  assigneeFilter.innerHTML = "";
  const options = [
    { value: "", label: "Все исполнители" },
    { value: "__unassigned__", label: "Не назначено" },
    ...assignees.map((name) => ({ value: name, label: name }))
  ];

  options.forEach((item) => {
    const option = document.createElement("option");
    option.value = item.value;
    option.textContent = item.label;
    assigneeFilter.appendChild(option);
  });

  assigneeFilter.value = options.some((item) => item.value === current) ? current : "";
}

function createTaskCard(task) {
  const card = document.createElement("article");
  card.className = "card";
  card.draggable = true;
  card.dataset.taskId = String(task.id);
  card.innerHTML = `
    <div class="card-top">
      <strong>#${task.id} ${task.title}</strong>
      <div class="inline-row">
        <button type="button" class="secondary tiny" data-edit="${task.id}">Изменить</button>
        <button type="button" class="danger tiny" data-delete="${task.id}">x</button>
      </div>
    </div>
    <p>${task.description || "Без описания"}</p>
    <div class="meta">Исп.: ${task.assignee || "-"}</div>
    <div class="meta">Срок: ${fmtDate(task.end_date)}</div>
    <div class="meta">Прогресс: ${task.progress}%</div>
  `;

  card.addEventListener("dragstart", (event) => {
    event.dataTransfer.setData("text/plain", String(task.id));
  });

  return card;
}

function renderKanban() {
  const filter = assigneeFilter.value;
  const columns = [0, 1, 2, 3].map((status) => {
    const section = document.createElement("section");
    section.className = "column";
    section.dataset.status = String(status);
    section.innerHTML = `<h3>${STATUS[status]}</h3><div class="column-inner"></div>`;

    section.addEventListener("dragover", (event) => event.preventDefault());
    section.addEventListener("drop", async (event) => {
      event.preventDefault();
      const taskId = Number(event.dataTransfer.getData("text/plain"));
      await moveTask(taskId, status);
    });
    return section;
  });

  kanbanBoard.innerHTML = "";

  if (!state.tasks.length) {
    kanbanBoard.innerHTML = '<div class="empty-state">В проекте пока нет задач</div>';
    return;
  }

  columns.forEach((column) => kanbanBoard.appendChild(column));

  state.tasks
    .filter((task) => {
      const assignee = (task.assignee || "").trim();
      if (!filter) return true;
      if (filter === "__unassigned__") return !assignee;
      return assignee === filter;
    })
    .forEach((task) => {
      const column = columns[Number(task.status)] || columns[0];
      column.querySelector(".column-inner").appendChild(createTaskCard(task));
    });

  kanbanBoard.querySelectorAll("button[data-edit]").forEach((button) => {
    button.addEventListener("click", (event) => {
      event.preventDefault();
      event.stopPropagation();
      startTaskEdit(Number(button.dataset.edit));
    });
  });

  kanbanBoard.querySelectorAll("button[data-delete]").forEach((button) => {
    button.addEventListener("click", async (event) => {
      event.preventDefault();
      event.stopPropagation();
      const taskId = Number(button.dataset.delete);
      if (!confirm("Удалить задачу?")) return;
      try {
        await api(`/api/tasks/${taskId}`, { method: "DELETE" });
        if (state.editingTaskId === taskId) {
          resetTaskForm();
        }
        await refreshData();
      } catch (error) {
        console.error(error);
        alert(error.message || "Не удалось удалить задачу");
      }
    });
  });
}

function renderGantt() {
  const canvas = ganttCanvas;
  const ctx = canvas.getContext("2d");
  const tasks = state.gantt.tasks || [];
  const critical = new Set(state.gantt.critical_path || []);
  const bottlenecks = new Set(state.gantt.bottlenecks || []);

  ctx.clearRect(0, 0, canvas.width, canvas.height);
  ctx.fillStyle = "#fbf8ef";
  ctx.fillRect(0, 0, canvas.width, canvas.height);

  if (!tasks.length) {
    ctx.fillStyle = "#5a5247";
    ctx.font = "16px Manrope";
    ctx.fillText("В проекте пока нет задач", 20, 40);
    return;
  }

  const minStart = Math.min(...tasks.map((task) => task.start_date || task.end_date || 0));
  const maxEnd = Math.max(...tasks.map((task) => task.end_date || task.start_date || 0));
  const span = Math.max(1, maxEnd - minStart);
  const left = 220;
  const right = 20;
  const top = 40;
  const rowH = 34;
  const barH = 18;
  const width = canvas.width - left - right;

  ctx.strokeStyle = "#d7d1c2";
  ctx.fillStyle = "#665e4f";
  ctx.font = "12px JetBrains Mono";
  for (let i = 0; i <= 8; i += 1) {
    const x = left + (i / 8) * width;
    const ts = Math.floor(minStart + (i / 8) * span);
    ctx.beginPath();
    ctx.moveTo(x, top - 20);
    ctx.lineTo(x, top + tasks.length * rowH + 10);
    ctx.stroke();
    ctx.fillText(new Date(ts * 1000).toLocaleDateString("ru-RU"), x - 24, top - 24);
  }

  const byId = new Map(tasks.map((task, index) => [task.id, { task, index }]));

  tasks.forEach((task, index) => {
    const y = top + index * rowH;
    const x1 = left + ((task.start_date - minStart) / span) * width;
    const x2 = left + ((task.end_date - minStart) / span) * width;
    const barW = Math.max(12, x2 - x1);

    ctx.fillStyle = critical.has(task.id) ? "#d94829" : "#2f6b73";
    ctx.fillRect(x1, y + 8, barW, barH);

    if (bottlenecks.has(task.id)) {
      ctx.strokeStyle = "#111";
      ctx.lineWidth = 2;
      ctx.strokeRect(x1, y + 8, barW, barH);
      ctx.lineWidth = 1;
    }

    ctx.fillStyle = "#24211d";
    ctx.font = "13px Manrope";
    ctx.fillText(`#${task.id} ${task.title}`, 8, y + 22);
  });

  ctx.strokeStyle = "#232323";
  tasks.forEach((task) => {
    (task.dependencies || []).forEach((depId) => {
      const from = byId.get(depId);
      const to = byId.get(task.id);
      if (!from || !to) return;

      const fromX = left + ((from.task.end_date - minStart) / span) * width;
      const fromY = top + from.index * rowH + 17;
      const toX = left + ((to.task.start_date - minStart) / span) * width;
      const toY = top + to.index * rowH + 17;

      ctx.beginPath();
      ctx.moveTo(fromX, fromY);
      ctx.lineTo((fromX + toX) / 2, fromY);
      ctx.lineTo((fromX + toX) / 2, toY);
      ctx.lineTo(toX, toY);
      ctx.stroke();

      ctx.beginPath();
      ctx.moveTo(toX - 6, toY - 4);
      ctx.lineTo(toX, toY);
      ctx.lineTo(toX - 6, toY + 4);
      ctx.stroke();
    });
  });
}

function renderAnalytics() {
  const analytics = state.analytics;
  if (!analytics) return;

  document.getElementById("metricDone").textContent = `${analytics.percent_done}%`;
  document.getElementById("metricAvgProgress").textContent = `${analytics.avg_progress}%`;
  document.getElementById("metricOverdue").textContent = String(analytics.overdue_tasks);
  document.getElementById("metricForecast").textContent = analytics.forecast_completion ? fmtDate(analytics.forecast_completion) : "-";

  const labels = (analytics.assignee_load || []).map((item) => item.assignee);
  const values = (analytics.assignee_load || []).map((item) => item.tasks);
  const canvas = document.getElementById("assigneeChart");
  const ctx = canvas.getContext("2d");

  if (state.chart) {
    state.chart.destroy();
    state.chart = null;
  }

  if (!analytics.total_tasks) {
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    ctx.fillStyle = "#5a5247";
    ctx.font = "16px Manrope";
    ctx.fillText("Недостаточно данных для аналитики", 20, 40);
    return;
  }

  state.chart = new Chart(ctx, {
    type: "bar",
    data: {
      labels,
      datasets: [{
        label: "Нагрузка по исполнителям",
        data: values,
        backgroundColor: ["#f2a65a", "#5b8e7d", "#bc4b51", "#6c757d", "#9c6644"]
      }]
    },
    options: {
      responsive: true,
      plugins: { legend: { display: false } },
      scales: {
        y: {
          beginAtZero: true,
          title: {
            display: true,
            text: "Количество задач"
          }
        },
        x: {
          title: {
            display: true,
            text: "Исполнители"
          }
        }
      }
    }
  });
}

function renderEvents() {
  commandsList.innerHTML = "";
  notificationsList.innerHTML = "";

  if (!state.commands.length) {
    const empty = document.createElement("li");
    empty.textContent = "Команд пока нет";
    commandsList.appendChild(empty);
  } else {
    state.commands.forEach((event) => {
      const li = document.createElement("li");
      li.textContent = `${fmtDate(event.created_at)} | #${event.task_id} | ${event.command_type} | ${event.payload}`;
      commandsList.appendChild(li);
    });
  }

  if (!state.notifications.length) {
    const empty = document.createElement("li");
    empty.textContent = "Уведомлений нет";
    notificationsList.appendChild(empty);
  } else {
    state.notifications.forEach((event) => {
      const li = document.createElement("li");
      li.textContent = `${fmtDate(event.created_at)} | ${event.assignee} | ${event.kind} | ${event.message}`;
      notificationsList.appendChild(li);
    });
  }
}

async function moveTask(taskId, status) {
  await api(`/api/tasks/${taskId}/move`, {
    method: "POST",
    body: JSON.stringify({ status })
  });
  await refreshData();
}

async function loadProjects() {
  state.projects = await api("/api/projects");
  if (!state.projects.length) {
    const now = Math.floor(Date.now() / 1000);
    const created = await api("/api/projects", {
      method: "POST",
      body: JSON.stringify({
        name: "Менеджер проектов",
        description: "Базовый проект",
        start_date: now,
        end_date: now + 7 * 86400
      })
    });
    state.projects = [created];
  }

  if (!state.currentProjectId || !state.projects.some((project) => project.id === state.currentProjectId)) {
    state.currentProjectId = state.projects[0].id;
  }
}

async function loadProjectDetails() {
  if (!state.currentProjectId) {
    state.tasks = [];
    state.gantt = { tasks: [], critical_path: [], bottlenecks: [] };
    state.analytics = { total_tasks: 0, done_tasks: 0, percent_done: 0, avg_progress: 0, overdue_tasks: 0, forecast_completion: 0, assignee_load: [] };
    state.commands = [];
    state.notifications = [];
    return;
  }

  const id = state.currentProjectId;
  state.tasks = await api(`/api/projects/${id}/tasks`);
  state.gantt = await api(`/api/projects/${id}/gantt`);
  state.analytics = await api(`/api/projects/${id}/analytics`);
  state.commands = await api(`/api/projects/${id}/commands`);
  state.notifications = await api(`/api/projects/${id}/notifications`);
}

function renderAll() {
  renderProjects();
  renderAssigneeFilter();
  renderKanban();
  renderGantt();
  renderAnalytics();
  renderEvents();
}

async function refreshData() {
  try {
    await loadProjects();
    await loadProjectDetails();
    renderAll();
  } catch (error) {
    console.error(error);
    alert(error.message || "Ошибка загрузки данных. Проверьте, запущен ли сервер.");
  }
}

projectSelect.addEventListener("change", async () => {
  state.currentProjectId = Number(projectSelect.value);
  setProjectFormMode(false);
  await refreshData();
});

newProjectBtn.addEventListener("click", () => {
  resetProjectFormForCreate();
});

editProjectBtn.addEventListener("click", () => {
  const current = state.projects.find((project) => project.id === state.currentProjectId);
  if (!current) return;
  document.getElementById("projectName").value = current.name || "";
  document.getElementById("projectDescription").value = current.description || "";
  document.getElementById("projectStart").value = fromTs(current.start_date);
  document.getElementById("projectEnd").value = fromTs(current.end_date);
  setProjectFormMode(true);
});

deleteProjectBtn.addEventListener("click", async () => {
  if (!state.currentProjectId) return;
  if (!confirm("Удалить проект и все задачи?")) return;
  try {
    await api(`/api/projects/${state.currentProjectId}`, { method: "DELETE" });
    state.currentProjectId = null;
    setProjectFormMode(false);
    resetProjectFormForCreate();
    await refreshData();
  } catch (error) {
    console.error(error);
    alert(error.message || "Не удалось удалить проект");
  }
});

projectForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  try {
    const payload = {
      name: document.getElementById("projectName").value.trim(),
      description: document.getElementById("projectDescription").value.trim(),
      start_date: toTs(document.getElementById("projectStart").value),
      end_date: toTs(document.getElementById("projectEnd").value)
    };

    if (!payload.name) {
      alert("Введите название проекта");
      return;
    }

    if (state.projectEditMode && state.currentProjectId) {
      await api(`/api/projects/${state.currentProjectId}`, {
        method: "PUT",
        body: JSON.stringify(payload)
      });
    } else {
      const created = await api("/api/projects", {
        method: "POST",
        body: JSON.stringify(payload)
      });
      state.currentProjectId = created.id;
    }

    setProjectFormMode(false);
    await refreshData();
  } catch (error) {
    console.error(error);
    alert(error.message || "Не удалось сохранить проект");
  }
});

projectCancelEditBtn.addEventListener("click", () => {
  setProjectFormMode(false);
  renderProjects();
});

taskForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  if (!state.currentProjectId) return;

  try {
    const payload = {
      title: document.getElementById("taskTitle").value.trim(),
      description: document.getElementById("taskDescription").value.trim(),
      assignee: document.getElementById("taskAssignee").value.trim(),
      status: Number(document.getElementById("taskStatus").value),
      start_date: toTs(document.getElementById("taskStart").value),
      end_date: toTs(document.getElementById("taskEnd").value),
      progress: Number(document.getElementById("taskProgress").value || 0)
    };

    let task;
    if (state.editingTaskId) {
      task = await api(`/api/tasks/${state.editingTaskId}`, {
        method: "PUT",
        body: JSON.stringify(payload)
      });
    } else {
      task = await api(`/api/projects/${state.currentProjectId}/tasks`, {
        method: "POST",
        body: JSON.stringify(payload)
      });
    }

    const depValue = Number(document.getElementById("taskDependency").value || 0);
    if (depValue > 0 && depValue !== task.id) {
      await api(`/api/tasks/${task.id}/dependencies`, {
        method: "POST",
        body: JSON.stringify({ depends_on_task_id: depValue })
      });
    }

    resetTaskForm();
    await refreshData();
  } catch (error) {
    console.error(error);
    alert(error.message || "Не удалось сохранить задачу");
  }
});

taskCancelEditBtn.addEventListener("click", () => {
  resetTaskForm();
});

assigneeFilter.addEventListener("change", renderKanban);
refreshBtn.addEventListener("click", refreshData);

document.querySelectorAll(".tab-btn").forEach((button) => {
  button.addEventListener("click", () => {
    const target = button.dataset.tab;
    document.querySelectorAll(".tab-btn").forEach((item) => item.classList.remove("active"));
    button.classList.add("active");
    document.querySelectorAll(".tab-content").forEach((tab) => tab.classList.remove("active"));
    document.getElementById(`tab-${target}`).classList.add("active");
  });
});

resetProjectFormForCreate();
refreshData();
