#include <iostream>
#include <libpq-fe.h> // PostgreSQL C Library

using namespace std;

// Asegúrate de reemplazar estos valores con tu IP y credenciales
#define DB_HOST "10.0.20.20" // IP de la VM 5 (Base de Datos)
#define DB_USER "servidor"
#define DB_PASS "1234"
#define DB_NAME "datasets"

int main() {
    PGconn *conn = NULL;
    PGresult *res = NULL;
    string conninfo;

    // Construir la cadena de conexión
    conninfo = "host=" + string(DB_HOST) + " user=" + string(DB_USER) + 
               " password=" + string(DB_PASS) + " dbname=" + string(DB_NAME);

    cout << "Intentando conectar a DB: " << DB_HOST << endl;

    // 1. Conectar
    conn = PQconnectdb(conninfo.c_str());

    if (PQstatus(conn) != CONNECTION_OK) {
        cerr << "--- ERROR: FALLO LA CONEXIÓN A POSTGRES ---" << endl;
        cerr << "Mensaje del servidor: " << PQerrorMessage(conn) << endl;
        PQfinish(conn);
        return 1;
    }

    cout << "¡ÉXITO! Conexión establecida." << endl;

    // 2. Ejecutar una consulta simple
    res = PQexec(conn, "SELECT count(*) FROM dataset1;");

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        cerr << "--- ERROR: Fallo la consulta SELECT ---" << endl;
        PQclear(res);
        PQfinish(conn);
        return 1;
    }

    // 3. Leer el resultado
    cout << "Total de filas en el dataset: " << PQgetvalue(res, 0, 0) << endl;

    // 4. Limpiar y cerrar
    PQclear(res);
    PQfinish(conn);
    return 0;
}
