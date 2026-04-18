"""
AgroTag Network — Dashboard Local (Terminal)
============================================
Exibe os dados mais recentes de cada nó em tempo real.
Atualiza a cada 5 segundos.

Uso:
    python dashboard.py
    python dashboard.py --db agrotag.db --interval 10
"""

import sqlite3
import time
import os
import argparse
from datetime import datetime

DB_PATH          = "agrotag.db"
REFRESH_INTERVAL = 5   # segundos
NODE_COUNT       = 4

HEADER = """
╔══════════════════════════════════════════════════════════════════════════════════╗
║                       🌱  Agro sustain Network — Dashboard                          ║
╚══════════════════════════════════════════════════════════════════════════════════╝
"""

NODE_TEMPLATE = """
  ┌─ Nó {node_id} ─────────────────────────────────────────────────────────────────┐
  │  Recebido:    {received_at}   RSSI: {rssi} dBm   SNR: {snr} dB
  │
  │  💧 Umidade solo:    {soil_moisture:>7.1f} %        🌡️  Temp. solo:    {soil_temperature:>7.1f} °C
  │  💨 Umidade ar:      {air_humidity:>7.1f} %        ⚡  EC:            {ec:>7.1f} µS/cm
  │  🧪 pH:              {ph:>7.2f}            
  │
  │  🌿 Nitrogênio (N):  {nitrogen:>7.1f} mg/kg
  │  🌿 Fósforo (P):     {phosphorus:>7.1f} mg/kg
  │  🌿 Potássio (K):    {potassium:>7.1f} mg/kg
  └───────────────────────────────────────────────────────────────────────────────┘"""

NO_DATA_TEMPLATE = """
  ┌─ Nó {node_id} ─────────────────────────────────────────────────────────────────┐
  │  ⏳  Aguardando dados...
  └───────────────────────────────────────────────────────────────────────────────┘"""


def get_latest(conn: sqlite3.Connection) -> dict:
    """Retorna o registro mais recente de cada nó."""
    cursor = conn.execute("""
                          SELECT
                              node_id, received_at,
                              soil_moisture, soil_temperature, air_humidity,
                              ec, ph, nitrogen, phosphorus, potassium,
                              rssi, snr
                          FROM sensor_readings
                          WHERE id IN (
                              SELECT MAX(id) FROM sensor_readings GROUP BY node_id
                          )
                          ORDER BY node_id
                          """)
    rows = {}
    for row in cursor.fetchall():
        rows[row[0]] = {
            "node_id":          row[0],
            "received_at":      row[1],
            "soil_moisture":    row[2],
            "soil_temperature": row[3],
            "air_humidity":     row[4],
            "ec":               row[5],
            "ph":               row[6],
            "nitrogen":         row[7],
            "phosphorus":       row[8],
            "potassium":        row[9],
            "rssi":             row[10],
            "snr":              row[11],
        }
    return rows


def get_stats(conn: sqlite3.Connection) -> dict:
    """Retorna estatísticas gerais do banco."""
    cursor = conn.execute("SELECT COUNT(*), MAX(received_at) FROM sensor_readings")
    row = cursor.fetchone()
    return {
        "total":   row[0] or 0,
        "last_at": row[1] or "—",
    }


def clear():
    os.system("clear" if os.name == "posix" else "cls")


def render(conn: sqlite3.Connection):
    clear()
    latest = get_latest(conn)
    stats  = get_stats(conn)

    print(HEADER)
    print(f"  🕐  Atualizado: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}   "
          f"📦 Total de leituras: {stats['total']}")

    for node_id in range(1, NODE_COUNT + 1):
        if node_id in latest:
            print(NODE_TEMPLATE.format(**latest[node_id]))
        else:
            print(NO_DATA_TEMPLATE.format(node_id=node_id))

    print(f"\n  ℹ️  Banco: agrotag.db   |   Pressione Ctrl+C para sair\n")


def main():
    parser = argparse.ArgumentParser(description="AgroTag Dashboard")
    parser.add_argument("--db",       default=DB_PATH,          help="Caminho do banco SQLite")
    parser.add_argument("--interval", default=REFRESH_INTERVAL, type=int,
                        help="Intervalo de atualização em segundos")
    args = parser.parse_args()

    conn = sqlite3.connect(args.db, check_same_thread=False)

    print("Iniciando dashboard... (Ctrl+C para sair)")

    try:
        while True:
            render(conn)
            time.sleep(args.interval)
    except KeyboardInterrupt:
        print("\nDashboard encerrado.")
    finally:
        conn.close()


if __name__ == "__main__":
    main()