#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <queue>
#include <chrono>
#include <random>
#include <atomic>
#include <map>

using namespace std;
using namespace chrono;


// -------- Estrutura do recurso compartilhado --------
struct Recurso {
    string nome;
    atomic<bool> bloqueado;
    int dono; // ID da thread que possui o recurso
    int dono_timestamp;
    mutex mtx;
    queue<int> fila_espera;

    Recurso(string nome_) : nome(nome_), bloqueado(false), dono(-1), dono_timestamp(-1) {}
};

Recurso X("X");
Recurso Y("Y");

mutex global_mutex; // para prints e controle centralizado
atomic<int> timestamp_counter(0); // contador de timestamps

// -------- Estado das threads --------
struct Transacao {
    int id;
    int timestamp;
    thread th;

    Transacao(int id_) : id(id_) {
        timestamp = timestamp_counter++;
    }
};

map<int, int> estados; // 0 = running, 1 = esperando, 2 = abortado, 3 = finalizado
mutex estados_mtx;

// -------- Utilitários --------
void log(int id, const string& msg) {
    lock_guard<mutex> lock(global_mutex);
    cout << "T" << id << ": " << msg << endl;
}

void sleep_random() {
    static random_device rd;
    static mt19937 gen(rd());
    static uniform_int_distribution<> dist(100, 500); // 100 a 500 ms
    this_thread::sleep_for(milliseconds(dist(gen)));
}

// -------- Função de lock com wait-die --------
bool tentar_lock(Recurso& r, int tid, int timestamp) {
    lock_guard<mutex> lock(r.mtx);
    if (!r.bloqueado) {
        r.bloqueado = true;
        r.dono = tid;
        r.dono_timestamp = timestamp;
        log(tid, "obteve bloqueio de " + r.nome);
        return true;
    } else {
        log(tid, "tentou bloquear " + r.nome + " (ocupado por T" + to_string(r.dono) + ")");
        // Algoritmo Wait-Die
        if (timestamp < r.dono_timestamp) {
            log(tid, "esperando por " + r.nome + " (wait-die)");
            r.fila_espera.push(tid);
            {
                lock_guard<mutex> lock(estados_mtx);
                estados[tid] = 1; // esperando
            }
            return false; // vai esperar, mas na simulação vamos abortar
        } else {
            log(tid, "foi finalizada devido a deadlock (abortada pelo wait-die), recurso " + r.nome);
            return false; // aborta
        }
    }
}

void liberar(Recurso& r, int tid) {
    lock_guard<mutex> lock(r.mtx);
    if (r.dono == tid) {
        r.bloqueado = false;
        r.dono = -1;
        r.dono_timestamp = -1;
        log(tid, "liberou " + r.nome);
    }
}

// -------- Função da thread --------
void executar_transacao(int id) {
REINICIO:
    int timestamp = timestamp_counter++;
    {
        lock_guard<mutex> lock(estados_mtx);
        estados[id] = 0; // executando
    }

    log(id, "entrou em execução com timestamp " + to_string(timestamp));
    sleep_random();

    // Tenta pegar X
    bool lock_x = tentar_lock(X, id, timestamp);
    if (!lock_x) {
        {
            lock_guard<mutex> lock(estados_mtx);
            estados[id] = 2; // abortado
        }
        log(id, "reiniciando...");
        sleep_random();
        goto REINICIO;
    }

    sleep_random();

    // Tenta pegar Y
    bool lock_y = tentar_lock(Y, id, timestamp);
    if (!lock_y) {
        liberar(X, id);
        {
            lock_guard<mutex> lock(estados_mtx);
            estados[id] = 2;
        }
        log(id, "reiniciando...");
        sleep_random();
        goto REINICIO;
    }

    // Operação crítica simulada
    sleep_random();

    liberar(X, id);
    sleep_random();
    liberar(Y, id);
    sleep_random();

    {
        lock_guard<mutex> lock(estados_mtx);
        estados[id] = 3; // finalizado
    }
    log(id, "finalizou sua execução com sucesso.");
}

// -------- Função principal --------
int main() {
    
    const int N = 5; // número de transações
    vector<Transacao> transacoes;

    for (int i = 0; i < N; ++i) {
        transacoes.emplace_back(i);
    }

    for (auto& t : transacoes) {
        t.th = thread(executar_transacao, t.id);
    }

    for (auto& t : transacoes) {
        t.th.join();
    }

    cout << "\nTodas as transações terminaram.\n";
    return 0;
}
