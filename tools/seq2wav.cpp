// Renderiza un sonido del BRSAR a WAV: seq2wav archivo.brsar NOMBRE salida.wav [segundos]
#include "../snd/brsar.hpp"
#include "../snd/seq.hpp"
#include <cstdio>
int main(int argc, char** argv) {
    if (argc < 4) { fprintf(stderr, "uso: seq2wav x.brsar NOMBRE out.wav [seg]\n"); return 1; }
    snd::Brsar B; if (!B.load(argv[1])) { fprintf(stderr, "brsar\n"); return 1; }
    const snd::SoundInfo* si = B.sound(argv[2]); if (!si) { fprintf(stderr, "sonido no encontrado\n"); return 1; }
    size_t sl, bl, wl; const uint8_t* sd = B.file(si->fileId, sl); uint32_t bf = B.bankFile[si->bankId]; const uint8_t* bd = B.file(bf, bl); const uint8_t* wd = B.waveArc(bf, wl);
    printf("%s: seq file %u (%zu B) off %u, banco %u -> file %u (%zu B), ondas %zu B\n", si->name.c_str(), si->fileId, sl, si->seqOffset, si->bankId, bf, bl, wl);
    snd::Bank bank; if (!bank.load(bd, bl)) { fprintf(stderr, "rbnk\n"); return 1; } snd::WaveArc wa; if (!wa.load(wd, wl)) { fprintf(stderr, "rwar\n"); return 1; }
    snd::Sequencer S; if (!S.start(sd, sl, si->seqOffset, &bank, &wa, si->allocTrack)) { fprintf(stderr, "rseq\n"); return 1; }
    float secs = argc > 4 ? atof(argv[4]) : 30; int total = (int)(secs * snd::OUT_RATE); std::vector<float> buf(total * 2, 0.0f);
    int done = 0; while (done < total && !S.finished()) { int n = std::min(1024, total - done); S.render(&buf[done * 2], n); done += n; }
    int notes = 0; for (auto& v : S.voices) (void)v; double peak = 0, rms = 0; for (int i = 0; i < done * 2; i++) { peak = std::max(peak, (double)fabs(buf[i])); rms += buf[i] * buf[i]; } rms = sqrt(rms / std::max(1, done * 2));
    printf("renderizado %.1f s, pico %.3f, rms %.4f, tempo %d timebase %d, terminado=%d\n", done / (float)snd::OUT_RATE, peak, rms, S.tempo, S.timebase, (int)S.finished());
    FILE* f = fopen(argv[3], "wb"); uint32_t dataLen = done * 4; uint32_t rate = snd::OUT_RATE, brate = rate * 4; uint16_t ch = 2, bits = 16, align = 4, fmt = 1; uint32_t riffLen = 36 + dataLen, fmtLen = 16;
    fwrite("RIFF", 1, 4, f); fwrite(&riffLen, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&fmtLen, 4, 1, f); fwrite(&fmt, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&rate, 4, 1, f); fwrite(&brate, 4, 1, f); fwrite(&align, 2, 1, f); fwrite(&bits, 2, 1, f); fwrite("data", 1, 4, f); fwrite(&dataLen, 4, 1, f);
    float g = peak > 0.9 ? 0.9f / (float)peak : 1.0f; for (int i = 0; i < done * 2; i++) { int16_t s = (int16_t)std::max(-32767.0f, std::min(32767.0f, buf[i] * g * 32767.0f)); fwrite(&s, 2, 1, f); }
    fclose(f); (void)notes; return 0;
}
