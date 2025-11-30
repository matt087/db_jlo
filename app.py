#!/usr/bin/env python3
import socket
import struct

# ======== CONFIGURACIÓN ========
HOST = "10.0.20.1" 
PORT = 8080

MODE = 1 

def recvall(sock, nbytes):
    """
    Lee exactamente nbytes del socket.
    Lanza excepción si la conexión se cierra antes.
    """
    data = bytearray()
    while len(data) < nbytes:
        paquete = sock.recv(nbytes - len(data))
        if not paquete:
            raise ConnectionError("Conexión cerrada antes de recibir todos los datos")
        data.extend(paquete)
    return bytes(data)


def pedir_entero_positivo(mensaje):
    while True:
        try:
            valor = int(input(mensaje).strip())
            if valor <= 0:
                print("Debe ser un entero positivo.")
                continue
            return valor
        except ValueError:
            print("Entrada inválida, debe ser un número entero.")


def main():
    k = pedir_entero_positivo("Ingrese el número de clusters K: ")
    max_iters = pedir_entero_positivo("Ingrese el número máximo de iteraciones: ")

    # ---- 1) Crear socket y conectar ----
    with socket.create_connection((HOST, PORT)) as sock:
        print(f"\nConectado a {HOST}:{PORT}")

        # ---- 2) Enviar KMeansRequest (3 int) ----
        req_fmt = struct.Struct("@iii")  # k, max_iters, mode
        req_bytes = req_fmt.pack(k, max_iters, MODE)

        sock.sendall(req_bytes)
        print(f"Enviado request: K={k}, max_iters={max_iters}, mode={MODE}")

        header_fmt = struct.Struct("@diii4x")
        header_bytes = recvall(sock, header_fmt.size)

        cpu_time, iterations, k_server, dim = header_fmt.unpack(header_bytes)

        print("\n=== HEADER RECIBIDO ===")
        print(f"Tiempo CPU (segundos): {cpu_time:.6f}")
        print(f"Iteraciones reales   : {iterations}")
        print(f"K (servidor)         : {k_server}")
        print(f"Dimensión dataset    : {dim}")

        # ---- 4) Recibir cardinalidades ----
        card_bytes = recvall(sock, 4 * k_server)
        card_fmt = struct.Struct("@" + "i" * k_server)
        cardinalidades = card_fmt.unpack(card_bytes)

        print("\n=== CARDINALIDADES ===")
        for i, c in enumerate(cardinalidades):
            print(f"Cluster {i}: {c} puntos")

        print("\nCliente terminado correctamente.")


if __name__ == "__main__":
    main()
