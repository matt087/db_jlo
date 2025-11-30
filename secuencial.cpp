#include <iostream>
#include <vector>
#include <cmath>
#include <limits>
#include <cstring>
#include <algorithm>
#include <numeric>
#include <string>
#include <chrono> 

#include <libpq-fe.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdlib>
#include <cerrno>

using namespace std;

// Parámetros de BD y servidor
#define DB_HOST "10.0.20.20"
#define DB_USER "servidor"
#define DB_PASS "1234"
#define DB_NAME "datasets"
#define PORT    8080

// Estructuras de Datos
typedef vector<double> Point;

struct KMeansRequest {
    int k;
    int max_iters;
    int mode;  // no usado, pero se mantiene por compatibilidad
};

struct ResponseHeader {
    double cpu_time;   // tiempo de ejecución en segundos
    int    iterations; // iteraciones realmente ejecutadas
    int    k;          // número de clusters
    int    dim;        // dimensión del dataset
};

// ---------------------------------------------------------
// Distancia euclidiana al cuadrado
// ---------------------------------------------------------
double distancia_sq(const Point& p1, const Point& p2) {
    double sum = 0.0;
    for (size_t i = 0; i < p1.size(); ++i) {
        double diff = p1[i] - p2[i];
        sum += diff * diff;
    }
    return sum;
}

// ---------------------------------------------------------
// Cargar datos desde PostgreSQL
// ---------------------------------------------------------
vector<Point> cargar_dataset(int& dim) {
    vector<Point> datos;

    string conninfo = "host=" + string(DB_HOST) +
                      " user=" + string(DB_USER) +
                      " password=" + string(DB_PASS) +
                      " dbname=" + string(DB_NAME);

    PGconn* conn = PQconnectdb(conninfo.c_str());

    if (PQstatus(conn) != CONNECTION_OK) {
        cerr << "[ERROR DB] Fallo la conexión: " << PQerrorMessage(conn) << endl;
        PQfinish(conn);
        return datos;
    }

    // Ajusta la consulta al dataset que quieras usar
    PGresult* res = PQexec(conn, "SELECT v1, v2, v3, v4, v5 FROM dataset1;");

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        cerr << "[ERROR DB] Fallo en query: " << PQerrorMessage(conn) << endl;
        PQclear(res);
        PQfinish(conn);
        return datos;
    }

    int filas = PQntuples(res);
    dim = PQnfields(res); // aquí será 5

    for (int i = 0; i < filas; i++) {
        Point p;
        for (int j = 0; j < dim; j++) {
            p.push_back(std::stod(PQgetvalue(res, i, j)));
        }
        datos.push_back(p);
    }

    PQclear(res);
    PQfinish(conn);

    cout << "[DB] Datos cargados: " << datos.size()
         << " vectores de " << dim << " dimensiones." << endl;

    return datos;
}

// ---------------------------------------------------------
// ALGORITMO K-MEANS SECUENCIAL
// ---------------------------------------------------------
void ejecutar_kmeans_secuencial(int K, int max_iters, int socket_cliente) {
    int dim = 0;
    vector<Point> puntos = cargar_dataset(dim);
    if (puntos.empty() || dim == 0) {
        cerr << "[ERROR] Dataset vacío o dimensión 0." << endl;
        return;
    }

    size_t num_puntos = puntos.size();

    // Inicialización simple: primeros K puntos como centroides
    vector<Point> centroides(K);
    for (int i = 0; i < K; ++i) {
        centroides[i] = puntos[i];
    }

    // Asignación de cada punto a un cluster
    vector<int> asignaciones(num_puntos, -1);

    // Para guardar la cardinalidad final de cada cluster
    vector<int> conteos_final(K, 0);

    auto start = std::chrono::high_resolution_clock::now();
    int iter_real = 0;

    for (int iter = 0; iter < max_iters; ++iter) {
        bool hubo_cambios = false;

        // ASIGNACIÓN DE PUNTOS A CENTROIDES (SECUENCIAL)
        for (size_t i = 0; i < num_puntos; ++i) {
            double min_dist = numeric_limits<double>::max();
            int mejor_cluster = -1;

            for (int j = 0; j < K; ++j) {
                double dist = distancia_sq(puntos[i], centroides[j]);
                if (dist < min_dist) {
                    min_dist = dist;
                    mejor_cluster = j;
                }
            }

            if (asignaciones[i] != mejor_cluster) {
                asignaciones[i] = mejor_cluster;
                hubo_cambios = true;
            }
        }

        // RECALCULAR CENTROIDES (SECUENCIAL)
        vector<Point> nuevas_sumas(K, Point(dim, 0.0));
        vector<int> conteos(K, 0);

        for (size_t i = 0; i < num_puntos; ++i) {
            int id = asignaciones[i];
            if (id >= 0 && id < K) {
                conteos[id]++;
                for (int d = 0; d < dim; ++d) {
                    nuevas_sumas[id][d] += puntos[i][d];
                }
            }
        }

        for (int j = 0; j < K; ++j) {
            if (conteos[j] > 0) {
                for (int d = 0; d < dim; ++d) {
                    centroides[j][d] = nuevas_sumas[j][d] / conteos[j];
                }
            }
        }

        // Guardamos la cardinalidad de esta iteración
        conteos_final = conteos;
        iter_real = iter + 1;

        if (!hubo_cambios) {
            cout << "[Proceso] Convergencia alcanzada en iteración " << iter << endl;
            break;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    double tiempo_total =
        std::chrono::duration<double>(end - start).count();

    // Rellenar cabecera de respuesta
    ResponseHeader header;
    header.cpu_time   = tiempo_total;
    header.iterations = iter_real; // iteraciones realmente usadas
    header.k          = K;
    header.dim        = dim;

    // 1) Enviar header
    ssize_t sent_bytes = send(socket_cliente, &header, sizeof(header), 0);
    if (sent_bytes != sizeof(header)) {
        cerr << "[ERROR] No se pudo enviar completamente el header." << endl;
        return;
    }

    // 2) Enviar cardinalidades (K enteros)
    if (!conteos_final.empty()) {
        sent_bytes = send(socket_cliente, conteos_final.data(),
                          K * sizeof(int), 0);
        if (sent_bytes != static_cast<ssize_t>(K * sizeof(int))) {
            cerr << "[ERROR] No se pudieron enviar completamente las cardinalidades." << endl;
        }
    }

    // Logs en servidor
    cout << "MODO: SECUENCIAL - Tiempo: " << tiempo_total << " segundos" << endl;
    cout << "Cardinalidad de los clusters:" << endl;
    for (int j = 0; j < K; ++j) {
        cout << "  Cluster " << j << ": " << conteos_final[j] << " puntos" << endl;
    }
}

// ---------------------------------------------------------
// FUNCIÓN PRINCIPAL (MAIN)
// ---------------------------------------------------------
int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);

    // Crear socket TCP
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Fallo socket");
        exit(EXIT_FAILURE);
    }

    // Reutilizar puerto
    if (setsockopt(server_fd, SOL_SOCKET,
                   SO_REUSEADDR | SO_REUSEPORT,
                   &opt, sizeof(opt)) < 0) {
        perror("Fallo setsockopt");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    address.sin_family      = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port        = htons(PORT);

    // Vincular socket al puerto
    if (bind(server_fd,
             (struct sockaddr*)&address,
             sizeof(address)) < 0) {
        perror("Fallo bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Escuchar conexiones
    if (listen(server_fd, 5) < 0) {
        perror("Fallo listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    cout << "Servidor K-Means (SECUENCIAL) escuchando en puerto " << PORT << endl;

    // Bucle principal de aceptación de clientes
    while (true) {
        new_socket = accept(server_fd,
                            (struct sockaddr*)&address,
                            &addrlen);
        if (new_socket < 0) {
            perror("Accept");
            continue;
        }

        // Recibir la solicitud KMeansRequest
        KMeansRequest req;
        ssize_t read_bytes = read(new_socket, &req, sizeof(req));
        if (read_bytes != sizeof(req)) {
            cerr << "[ERROR] No se pudo leer correctamente KMeansRequest." << endl;
            close(new_socket);
            continue;
        }

        cout << "\n==============================================" << endl;
        cout << "NUEVA SOLICITUD - K=" << req.k
             << " ITERS=" << req.max_iters
             << " MODE=" << req.mode << endl;

        // Ejecutar K-Means secuencial y enviar resultados al cliente
        ejecutar_kmeans_secuencial(req.k, req.max_iters, new_socket);

        // Cerrar socket con el cliente
        close(new_socket);
    }

    // Cerrar socket servidor (en realidad nunca se llega aquí)
    close(server_fd);
    return 0;
}
