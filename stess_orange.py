import multiprocessing
import ors
import sys
import time


def burn():
    while True:
        pass


def read_temp():
    with open("/sys/class/thermal/thermal_zone0/temp") as f:
        return int(f.read()) / 1000


if __name__ == "__main__":
    duration = int(sys.argv[1]) if len(sys.argv) > 1 else None
    cores = os.cpu_count()
    if duration is None:
        print(f"Iniciando teste de estresse em {cores} núcleos (sem limite de tempo)")
    else:
        print(f"Iniciando teste de estresse em {cores} núcleos por {duration} s")

    processes = []
    for i in range(cores):
        p = multiprocessing.Process(target=burn, daemon=True)
        p.start()
        processes.append(p)

    try:
        start = time.time()
        while duration is None or time.time() - start < duration:
            temp = read_temp()
            elapsed = int(time.time() - start)
            print(f"{elapsed:5d}s  temperatura: {temp:.1f}C")
            if temp > 50:
                print("Muito quente, parando processo")
                break
            time.sleep(2)
    except KeyboardInterrupt:
        print("\nInterrompido pelo usuário")
    finally:
        for p in processes:
            p.terminate()
        print("Carga encerrada")