import socket

ip = "10.0.20.20"  # La IP de tu Base de Datos
puerto = 5432      # Puerto por defecto de Postgres

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.settimeout(3) # Esperar maximo 3 segundos

result = sock.connect_ex((ip, puerto))

if result == 0:
   print(f"¡ÉXITO! El puerto {puerto} en {ip} está ABIERTO.")
else:
   print(f"FALLO. No se puede conectar a {ip} en el puerto {puerto}.")

sock.close()
