import csv
import subprocess  # lets one program start another
import sys
import time
from pathlib import Path

DURACAO = 60  # segundos
CSV_PATH = Path("aircraft_log.csv")
CAMPOS = ["icao", "altitude", "callsign", "speed", "lat"]

if CSV_PATH.exists():
    CSV_PATH.unlink()  # apaga o CSV antigo

# abre o decodificador com o mesmo Python (mesmo venv) deste script
proc = subprocess.Popen([sys.executable, "adsb_decoder_csv.py"],
                        stdout=subprocess.DEVNULL)

inicio = time.monotonic()
primeira_vez = {}

try:
    while time.monotonic() - inicio < DURACAO:
        agora = time.monotonic() - inicio

        if proc.poll() is not None:  # o decodificador parou sozinho?
            print(f"\nO decodificador parou (código {proc.returncode})")
            break

        try:
            with open(CSV_PATH, newline="") as f:
                linhas = list(csv.DictReader(f))
        except FileNotFoundError:
            linhas = []

        for linha in linhas:
            if len(linha.get("last_seen") or "") != 19:
                continue  # linha meio escrita, pula
            icao = linha["icao"]
            for campo in CAMPOS:
                if linha.get(campo) and (icao, campo) not in primeira_vez:
                    primeira_vez[(icao, campo)] = agora
                    print(f"\r{agora:6.1f} s   {icao}   {campo}")

        print(f"\r{agora:6.1f} s", end="", flush=True)  # relógio na mesma linha
        time.sleep(0.5)  # só um sleep, uma vez por volta
finally:
    proc.terminate()  # pede para o decodificador parar
    proc.wait()       # espera até ele realmente parar
    print("\nDecodificador parado")