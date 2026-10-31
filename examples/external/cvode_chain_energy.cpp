// Minimal CVODE-only repro from the rc_scan test on SUNDIALS 7.9.0 (BSD, built outside this repository): energy of an undamped 4-mass spring chain after a long output step.
// usage: ./repro_chain METHOD(0=bdf,1=adams) MAXORD RTOL DT_OUT. BDF order 3 at rtol 6.14e-5, dt 3 gives an end/start energy ratio of 1.29 (order 1: 0.04, order 2: 0.99). Documented BDF behavior, not a bug.
// Minimal CVODE-only repro: 4 masses between two walls, springs k = 1,K,1,K,1; initial x0 = 3. Prints energy at the end vs start.
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cvode/cvode.h>
#include <nvector/nvector_serial.h>
#include <sunmatrix/sunmatrix_dense.h>
#include <sunlinsol/sunlinsol_dense.h>
static double K;
static int f(sunrealtype, N_Vector y, N_Vector yd, void*) {
  double* Y = N_VGetArrayPointer(y); double* F = N_VGetArrayPointer(yd); const int m = 4;
  for (int i = 0; i < m; ++i) { double kl = (i % 2 ? K : 1), kr = ((i + 1) % 2 ? K : 1);
    double l = i ? Y[i-1] : 0, r = i + 1 < m ? Y[i+1] : 0; F[i] = Y[m+i]; F[m+i] = kl*(l - Y[i]) + kr*(r - Y[i]); }
  return 0; }
static double E(const double* Y) { const int m = 4; double e = 0; for (int i = 0; i < m; ++i) { e += 0.5*Y[m+i]*Y[m+i]; double k = i%2?K:1; double d = Y[i]-(i?Y[i-1]:0); e += 0.5*k*d*d; }
  double k = m%2?K:1; e += 0.5*k*Y[m-1]*Y[m-1]; return e; }
int main(int argc, char** argv) {
  K = 50; int method = atoi(argv[1]); int maxord = atoi(argv[2]); double rtol = atof(argv[3]), dt = atof(argv[4]);
  SUNContext ctx; SUNContext_Create(SUN_COMM_NULL, &ctx); N_Vector y = N_VNew_Serial(8, ctx); double* Y = N_VGetArrayPointer(y);
  for (int i = 0; i < 8; ++i) Y[i] = 0; Y[0] = 3; double e0 = E(Y);
  void* cv = CVodeCreate(method ? CV_ADAMS : CV_BDF, ctx); CVodeInit(cv, f, 0, y); CVodeSStolerances(cv, rtol, 1e-10);
  CVodeSetMaxNumSteps(cv, 100000); CVodeSetMaxOrd(cv, maxord);
  SUNMatrix A = SUNDenseMatrix(8, 8, ctx); SUNLinearSolver LS = SUNLinSol_Dense(y, A, ctx); CVodeSetLinearSolver(cv, LS, A);
  double t = 0; for (int k = 1; k <= 34; ++k) { if (CVode(cv, k*dt, y, &t, CV_NORMAL) < 0) { puts("fail"); return 1; } }
  long ns; CVodeGetNumSteps(cv, &ns); printf("method=%s maxord=%d rtol=%g dt=%g: E0=%.3f E_end=%.3f ratio=%.4f steps=%ld\n", method?"adams":"bdf", maxord, rtol, dt, e0, E(Y), E(Y)/e0, ns);
}
