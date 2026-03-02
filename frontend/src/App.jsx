import { useState, useRef } from "react";
import axios from "axios";
import "./App.css";

const API = "http://localhost:8080";

export default function App() {
  const [input, setInput]   = useState("");
  const [output, setOutput] = useState("");
  const [loading, setLoading] = useState(false);
  const fileRef = useRef(null);

  // ── Ejecutar comandos ──
  const handleExecute = async () => {
    if(!input.trim()) return;
    setLoading(true);
    setOutput("");
    try {
      const res = await axios.post(`${API}/execute`, { commands: input });
      setOutput(res.data.output || "");
    } catch(err) {
      setOutput("Error de conexión con el backend.\n" + err.message);
    }
    setLoading(false);
  };

  // ── Cargar script .smia ──
  const handleFileLoad = (e) => {
    const file = e.target.files[0];
    if(!file) return;
    const reader = new FileReader();
    reader.onload = (ev) => setInput(ev.target.result);
    reader.readAsText(file);
    e.target.value = "";
  };

  // ── Limpiar ──
  const handleClear = () => {
    setInput("");
    setOutput("");
  };

  return (
    <div className="app">
      {/* ── HEADER ── */}
      <header className="header">
        <div className="header-title">
          <span className="header-icon">💾</span>
          <h1>ExtreamFS</h1>
          <span className="header-sub">Sistema de Archivos EXT2 — MIA 2026</span>
        </div>
        <div className="header-status">
          <span className="status-dot"></span>
          <span>Backend: localhost:8080</span>
        </div>
      </header>

      {/* ── MAIN ── */}
      <main className="main">

        {/* ── PANEL ENTRADA ── */}
        <section className="panel">
          <div className="panel-header">
            <span className="panel-title">📝 Entrada de Comandos</span>
            <div className="panel-actions">
              <button
                className="btn btn-secondary"
                onClick={() => fileRef.current.click()}
                title="Cargar script .smia"
              >
                📂 Cargar Script
              </button>
              <input
                ref={fileRef}
                type="file"
                accept=".smia,.txt"
                style={{ display: "none" }}
                onChange={handleFileLoad}
              />
              <button className="btn btn-danger" onClick={handleClear}>
                🗑️ Limpiar
              </button>
              <button
                className="btn btn-primary"
                onClick={handleExecute}
                disabled={loading}
              >
                {loading ? "⏳ Ejecutando..." : "▶ Ejecutar"}
              </button>
            </div>
          </div>
          <textarea
            className="textarea input-area"
            value={input}
            onChange={(e) => setInput(e.target.value)}
            placeholder={
              "# Escribe comandos aquí o carga un script .smia\n" +
              "mkdisk -size=10 -fit=f -unit=m -path=/home/disco.mia\n" +
              "fdisk -size=2 -path=/home/disco.mia -name=p1 -unit=m\n" +
              "mount -path=/home/disco.mia -name=p1\n" +
              "mkfs -id=541A\n" +
              "login -user=root -pass=123 -id=541A"
            }
            spellCheck={false}
          />
        </section>

        {/* ── PANEL SALIDA ── */}
        <section className="panel">
          <div className="panel-header">
            <span className="panel-title">📤 Salida</span>
            <div className="panel-actions">
              <button
                className="btn btn-secondary"
                onClick={() => setOutput("")}
              >
                🗑️ Limpiar
              </button>
              <button
                className="btn btn-secondary"
                onClick={() => navigator.clipboard.writeText(output)}
                disabled={!output}
              >
                📋 Copiar
              </button>
            </div>
          </div>
          <textarea
            className="textarea output-area"
            value={output}
            readOnly
            placeholder="Los resultados de los comandos aparecerán aquí..."
            spellCheck={false}
          />
        </section>
      </main>

      {/* ── FOOTER ── */}
      <footer className="footer">
        <span>Universidad San Carlos de Guatemala — Ingeniería en Sistemas — MIA 2026</span>
        <span>Carnet: 202200254</span>
      </footer>
    </div>
  );
}