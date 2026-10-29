void orbit_deriv(const double* y, double* dydt, int n, const double* m, double G) {
  for (int i = 0; i < n; ++i) {
    dydt[4*i+0] = y[4*i+2];
    dydt[4*i+1] = y[4*i+3];
    dydt[4*i+2] = 0.0;
    dydt[4*i+3] = 0.0;
  }
  const double eps2 = 0.09;
  for (int i = 0; i < n; ++i) {
    double xi = y[4*i+0];
    double yi = y[4*i+1];
    double axi = 0.0, ayi = 0.0;
    for (int j = 0; j < n; ++j) {
      if (j == i) continue;
      double dx = y[4*j+0] - xi;
      double dy = y[4*j+1] - yi;
      double r2 = dx*dx + dy*dy + eps2;
      double inv = 1.0 / std::sqrt(r2);
      double inv3 = inv*inv*inv;
      double s = G * m[j] * inv3;
      axi += s * dx;
      ayi += s * dy;
    }
    dydt[4*i+2] = axi;
    dydt[4*i+3] = ayi;
  }
}

void ref_step_orbit(const OrbitIn& in, OrbitOut* out) {
  int n = in.n;
  if (n < 2) n = 2;
  if (n > 8) n = 8;
  double dt = in.dt;
  double G = in.G;
  double m[8];
  for (int i = 0; i < 8; ++i) m[i] = in.m[i];

  int D = n * 4;
  std::vector<double> y(D), y5(D), y4e(D);
  for (int i = 0; i < n; ++i) {
    y[4*i+0] = in.x[i];
    y[4*i+1] = in.y[i];
    y[4*i+2] = in.vx[i];
    y[4*i+3] = in.vy[i];
  }

  // Dormand-Prince 5(4) coefficients
  const double c2=1.0/5.0, c3=3.0/10.0, c4=4.0/5.0, c5=8.0/9.0, c6=1.0, c7=1.0;
  const double a21=1.0/5.0;
  const double a31=3.0/40.0, a32=9.0/40.0;
  const double a41=44.0/45.0, a42=-56.0/15.0, a43=32.0/9.0;
  const double a51=19372.0/6561.0, a52=-25360.0/2187.0, a53=64448.0/6561.0, a54=-212.0/729.0;
  const double a61=9017.0/3168.0, a62=-355.0/33.0, a63=46732.0/5247.0, a64=49.0/176.0, a65=-5103.0/18656.0;
  const double a71=35.0/384.0, a73=500.0/1113.0, a74=125.0/192.0, a75=-2187.0/6784.0, a76=11.0/84.0;
  const double e1=71.0/57600.0;
  const double e3=-71.0/16695.0;
  const double e4=71.0/1920.0;
  const double e5=-17253.0/339200.0;
  const double e6=22.0/525.0;
  const double e7=-1.0/40.0;

  const double atol = 1e-13;
  const double rtol = 1e-12;
  const double safety = 0.9;
  const double minFac = 0.2;
  const double maxFac = 5.0;

  std::vector<double> k1(D), k2(D), k3(D), k4(D), k5(D), k6(D), k7(D), yt(D);

  double t = 0.0;
  double h = dt;
  // initial guess: limit h a bit if state is large/fast
  // keep h <= dt
  if (!(h > 0)) h = dt;
  if (h > dt) h = dt;
  if (h < 1e-14) h = dt;

  int iters = 0;
  const int maxIters = 20000;
  while (t < dt && iters < maxIters) {
    ++iters;
    if (t + h > dt) h = dt - t;
    if (h < 1e-15) { h = dt - t; }
    // stage 1
    orbit_deriv(y.data(), k1.data(), n, m, G);
    // stage 2
    for (int i = 0; i < D; ++i) yt[i] = y[i] + h*a21*k1[i];
    orbit_deriv(yt.data(), k2.data(), n, m, G);
    // stage 3
    for (int i = 0; i < D; ++i) yt[i] = y[i] + h*(a31*k1[i]+a32*k2[i]);
    orbit_deriv(yt.data(), k3.data(), n, m, G);
    // stage 4
    for (int i = 0; i < D; ++i) yt[i] = y[i] + h*(a41*k1[i]+a42*k2[i]+a43*k3[i]);
    orbit_deriv(yt.data(), k4.data(), n, m, G);
    // stage 5
    for (int i = 0; i < D; ++i) yt[i] = y[i] + h*(a51*k1[i]+a52*k2[i]+a53*k3[i]+a54*k4[i]);
    orbit_deriv(yt.data(), k5.data(), n, m, G);
    // stage 6
    for (int i = 0; i < D; ++i) yt[i] = y[i] + h*(a61*k1[i]+a62*k2[i]+a63*k3[i]+a64*k4[i]+a65*k5[i]);
    orbit_deriv(yt.data(), k6.data(), n, m, G);
    // 5th order result
    for (int i = 0; i < D; ++i) y5[i] = y[i] + h*(a71*k1[i]+a73*k3[i]+a74*k4[i]+a75*k5[i]+a76*k6[i]);
    // stage 7 (FSAL at y5)
    orbit_deriv(y5.data(), k7.data(), n, m, G);
    // error estimate
    double err2sum = 0.0;
    double errmax = 0.0;
    for (int i = 0; i < D; ++i) {
      double ei = h*(e1*k1[i]+e3*k3[i]+e4*k4[i]+e5*k5[i]+e6*k6[i]+e7*k7[i]);
      double sc = atol + rtol * std::max(std::fabs(y[i]), std::fabs(y5[i]));
      double r = ei / sc;
      err2sum += r*r;
      double ar = std::fabs(r);
      if (ar > errmax) errmax = ar;
    }
    double err = std::sqrt(err2sum / (double)D);
    if (!(err >= 0)) err = errmax;
    if (err <= 1.0) {
      t += h;
      y = y5;
      if (t >= dt) break;
      double fac = maxFac;
      if (err > 1e-16) fac = safety * std::pow(err, -0.2);
      if (fac < 1.0) fac = 1.0;
      if (fac > maxFac) fac = maxFac;
      h = h * fac;
      if (h > dt - t) h = dt - t;
    } else {
      double fac = safety * std::pow(err, -0.25);
      if (fac < minFac) fac = minFac;
      if (fac > 0.8) fac = 0.8;
      h = h * fac;
      if (h < 1e-15) {
        // accept to avoid stall, take minimum progress
        t += h;
        y = y5;
        h = 1e-15;
      }
      if (t + h > dt) h = dt - t;
      // guard against underflow to zero
      if (h < 1e-16) {
        h = dt - t;
        if (h < 1e-16) break;
        // take one Euler-safe tiny RK step: just accept y5 with recomputed small h
        // force accept next loop by temporarily loosening: do fixed small steps
        // To keep it simple, do a single RK4 with h and finish
      }
    }
    if (dt - t < 1e-15) break;
  }

  for (int i = 0; i < n; ++i) {
    out->x[i] = y[4*i+0];
    out->y[i] = y[4*i+1];
    out->vx[i] = y[4*i+2];
    out->vy[i] = y[4*i+3];
  }
  for (int i = n; i < 8; ++i) {
    out->x[i] = 0; out->y[i] = 0; out->vx[i] = 0; out->vy[i] = 0;
  }
}
