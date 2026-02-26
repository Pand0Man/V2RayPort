import json
import os
import queue
import subprocess
import threading
import tkinter as tk
from tkinter import filedialog, messagebox, ttk


class V2RayPortApp(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title("V2RayPort (Desktop MVP)")
        self.geometry("860x520")

        self.process: subprocess.Popen | None = None
        self.log_queue: queue.Queue[str] = queue.Queue()
        self.reader_thread: threading.Thread | None = None

        self.binary_path = tk.StringVar(value="")
        self.config_path = tk.StringVar(value="")
        self.status_var = tk.StringVar(value="Статус: остановлен")

        self._build_ui()
        self.after(120, self._flush_logs)

    def _build_ui(self) -> None:
        frame = ttk.Frame(self, padding=12)
        frame.pack(fill=tk.BOTH, expand=True)

        top = ttk.LabelFrame(frame, text="Настройки", padding=10)
        top.pack(fill=tk.X)

        ttk.Label(top, text="Бинарник v2ray/xray:").grid(row=0, column=0, sticky="w", pady=4)
        ttk.Entry(top, textvariable=self.binary_path, width=80).grid(row=0, column=1, padx=8)
        ttk.Button(top, text="Выбрать", command=self._pick_binary).grid(row=0, column=2)

        ttk.Label(top, text="JSON-конфиг:").grid(row=1, column=0, sticky="w", pady=4)
        ttk.Entry(top, textvariable=self.config_path, width=80).grid(row=1, column=1, padx=8)
        ttk.Button(top, text="Выбрать", command=self._pick_config).grid(row=1, column=2)

        controls = ttk.Frame(frame)
        controls.pack(fill=tk.X, pady=10)

        self.start_btn = ttk.Button(controls, text="Запустить", command=self.start)
        self.start_btn.pack(side=tk.LEFT)

        self.stop_btn = ttk.Button(controls, text="Остановить", command=self.stop, state=tk.DISABLED)
        self.stop_btn.pack(side=tk.LEFT, padx=8)

        ttk.Label(controls, textvariable=self.status_var).pack(side=tk.LEFT, padx=12)

        logs_frame = ttk.LabelFrame(frame, text="Логи", padding=10)
        logs_frame.pack(fill=tk.BOTH, expand=True)

        self.logs = tk.Text(logs_frame, wrap=tk.NONE, height=20)
        self.logs.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        scroll = ttk.Scrollbar(logs_frame, orient=tk.VERTICAL, command=self.logs.yview)
        scroll.pack(side=tk.RIGHT, fill=tk.Y)
        self.logs.configure(yscrollcommand=scroll.set)

    def _pick_binary(self) -> None:
        path = filedialog.askopenfilename(title="Выберите бинарник v2ray/xray")
        if path:
            self.binary_path.set(path)

    def _pick_config(self) -> None:
        path = filedialog.askopenfilename(
            title="Выберите JSON-конфиг",
            filetypes=[("JSON", "*.json"), ("All files", "*.*")],
        )
        if path:
            self.config_path.set(path)

    def _validate_inputs(self) -> bool:
        binary = self.binary_path.get().strip()
        config = self.config_path.get().strip()

        if not binary or not os.path.isfile(binary):
            messagebox.showerror("Ошибка", "Укажите корректный путь к бинарнику v2ray/xray")
            return False

        if not config or not os.path.isfile(config):
            messagebox.showerror("Ошибка", "Укажите корректный путь к JSON-конфигу")
            return False

        try:
            with open(config, "r", encoding="utf-8") as f:
                json.load(f)
        except json.JSONDecodeError as exc:
            messagebox.showerror("Ошибка", f"Некорректный JSON-конфиг: {exc}")
            return False
        except OSError as exc:
            messagebox.showerror("Ошибка", f"Не удалось прочитать конфиг: {exc}")
            return False

        return True

    def start(self) -> None:
        if self.process is not None:
            return

        if not self._validate_inputs():
            return

        binary = self.binary_path.get().strip()
        config = self.config_path.get().strip()

        try:
            self.process = subprocess.Popen(
                [binary, "run", "-config", config],
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                bufsize=1,
            )
        except OSError as exc:
            self.process = None
            messagebox.showerror("Ошибка запуска", str(exc))
            return

        self.status_var.set("Статус: запущен")
        self.start_btn.config(state=tk.DISABLED)
        self.stop_btn.config(state=tk.NORMAL)
        self._append_log("[V2RayPort] Процесс запущен\n")

        self.reader_thread = threading.Thread(target=self._read_stdout, daemon=True)
        self.reader_thread.start()

    def stop(self) -> None:
        if self.process is None:
            return

        self.process.terminate()
        try:
            self.process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait(timeout=2)

        self.process = None
        self.status_var.set("Статус: остановлен")
        self.start_btn.config(state=tk.NORMAL)
        self.stop_btn.config(state=tk.DISABLED)
        self._append_log("[V2RayPort] Процесс остановлен\n")

    def _read_stdout(self) -> None:
        assert self.process is not None and self.process.stdout is not None
        for line in self.process.stdout:
            self.log_queue.put(line)

        code = self.process.wait()
        self.log_queue.put(f"\n[V2RayPort] Процесс завершен с кодом {code}\n")
        self.process = None

    def _append_log(self, text: str) -> None:
        self.logs.insert(tk.END, text)
        self.logs.see(tk.END)

    def _flush_logs(self) -> None:
        try:
            while True:
                self._append_log(self.log_queue.get_nowait())
        except queue.Empty:
            pass

        if self.process is None:
            self.status_var.set("Статус: остановлен")
            self.start_btn.config(state=tk.NORMAL)
            self.stop_btn.config(state=tk.DISABLED)

        self.after(120, self._flush_logs)


if __name__ == "__main__":
    app = V2RayPortApp()
    app.mainloop()
