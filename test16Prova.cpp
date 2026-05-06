#include <iostream>
#include <cstring>
#include <cmath>
#include <cfloat>
using namespace std;

uint16_float toFloat16(float value);
float fromFloat16(uint16_t value);

bool aproximado(float original, float recuperado, float tolerancia = 0.001f) {
    return fabs(original - recuperado) <= fabs(original) * tolerancia;
}

void testar(const string& nome, float valor) {
    return fabs(original - recuperado) <= fabs(originl) * tolerancia;
}

void testar(const string& nome, float valor) {
    uint16_t f16 = toFloat16(valor);
    float recuperado = fromFloat16(f16);

    bool passou = aproximado(valor, recuperado);
    cout << (passou ? "[OK]  " : "[FALHOU] ");
    cout << nome << "\n";
    cout << "       original:   " << valor      << "\n";
    cout << "       float16:    0x" << hex << f16 << dec << "\n";
    cout << "       recuperado: " << recuperado  << "\n";
    cout << "       diferença:  " << fabs(valor - recuperado) << "\n\n";
}
void testarEspecial(const string& nome, float valor) {
    uint16_t f16       = toFloat16(valor);
    float    recuperado = fromFloat16(f16);

    // Para especiais, verificamos a propriedade, não o valor exato
    bool passou = false;
    if (isinf(valor))  passou = isinf(recuperado);
    if (isnan(valor))  passou = isnan(recuperado);

    cout << (passou ? "[OK]  " : "[FALHOU] ");
    cout << nome << "\n";
    cout << "       float16: 0x" << hex << f16 << dec << "\n\n";
}
int main() {

    cout << "=== Números comuns ===\n\n";
    testar("zero",          0.0f);
    testar("um",            1.0f);
    testar("negativo",     -1.0f);
    testar("pi",            3.14159f);
    testar("pequeno",       0.001f);
    testar("grande",        1000.0f);
    testar("negativo pi",  -3.14159f);

    cout << "=== Limites do float16 ===\n\n";
    testar("máximo float16",  65504.0f);  // maior número representável
    testar("mínimo positivo", 6e-5f);     // perto do menor representável

    cout << "=== Overflow (vira infinito) ===\n\n";
    // Qualquer valor acima de 65504 deve virar infinito no float16
    uint16_t f16_overflow  = toFloat16(99999.0f);
    float    rec_overflow  = fromFloat16(f16_overflow);
    cout << (isinf(rec_overflow) ? "[OK]  " : "[FALHOU] ");
    cout << "overflow 99999.0 → deve ser inf\n";
    cout << "       float16: 0x" << hex << f16_overflow << dec << "\n\n";

    cout << "=== Underflow (vira zero) ===\n\n";
    // Valores muito pequenos devem virar zero
    uint16_t f16_underflow = toFloat16(1e-8f);
    float    rec_underflow = fromFloat16(f16_underflow);
    cout << (rec_underflow == 0.0f ? "[OK]  " : "[FALHOU] ");
    cout << "underflow 1e-8 → deve ser zero\n";
    cout << "       float16: 0x" << hex << f16_underflow << dec << "\n\n";

    cout << "=== Valores especiais ===\n\n";
    testarEspecial("infinito positivo",  INFINITY);
    testarEspecial("infinito negativo", -INFINITY);
    testarEspecial("NaN",                NAN);

    cout << "=== Simetria (ida e volta) ===\n\n";
    // Converte para float16 e volta — deve ser aproximadamente igual
    float valores[] = { 0.5f, 1.5f, 2.0f, -0.5f, 100.0f, -100.0f };
    for (float v : valores) {
        testar("ida e volta " + to_string(v), v);
    }

    return 0;
}
