void ref_step_cartpole(const CartIn& in, CartOut* out) {
  double mc = in.mc;
  double mp = in.mp;
  double l = in.l;
  double g = in.g;
  double u = in.u;
  double x = in.x;
  double v = in.v;
  double th = in.th;
  double w = in.w;
  double dt = in.dt;

  // Derivatives: state s = [x, v, th, w], ds/dt = f(s)
  // Equations for point mass m=mp at distance l on massless rod,
  // cart mass M=mc, frictionless, force u on cart:
  //   (M + m*sin^2 th) * a = u + m*sin th*(l*w^2 - g*cos th)
  //   alpha = (g*sin th - a*cos th)/l
  auto accel = [&](double thi, double wi, double* a_out, double* al_out) {
    double s = std::sin(thi);
    double c = std::cos(thi);
    double den = mc + mp * s * s;
    double a = (u + mp * s * (l * wi * wi - g * c)) / den;
    double al = (g * s - a * c) / l;
    *a_out = a;
    *al_out = al;
  };

  // High-accuracy fixed-substep classical RK4 (constant u during dt).
  int N = 64;
  // keep substep h small for all allowed dt
  if (dt / (double)N > 0.0005) {
    N = (int)std::ceil(dt / 0.0005);
  }
  double h = dt / (double)N;

  for (int i = 0; i < N; ++i) {
    double a1, al1, a2, al2, a3, al3, a4, al4;
    accel(th, w, &a1, &al1);
    double k1x = v;
    double k1v = a1;
    double k1t = w;
    double k1w = al1;

    double v2 = v + 0.5 * h * k1v;
    double w2 = w + 0.5 * h * k1w;
    double th2 = th + 0.5 * h * k1t;
    accel(th2, w2, &a2, &al2);
    double k2x = v2;
    double k2v = a2;
    double k2t = w2;
    double k2w = al2;

    double v3 = v + 0.5 * h * k2v;
    double w3 = w + 0.5 * h * k2w;
    double th3 = th + 0.5 * h * k2t;
    accel(th3, w3, &a3, &al3);
    double k3x = v3;
    double k3v = a3;
    double k3t = w3;
    double k3w = al3;

    double v4 = v + h * k3v;
    double w4 = w + h * k3w;
    double th4 = th + h * k3t;
    accel(th4, w4, &a4, &al4);
    double k4x = v4;
    double k4v = a4;
    double k4t = w4;
    double k4w = al4;

    x += h * (k1x + 2.0 * k2x + 2.0 * k3x + k4x) / 6.0;
    v += h * (k1v + 2.0 * k2v + 2.0 * k3v + k4v) / 6.0;
    th += h * (k1t + 2.0 * k2t + 2.0 * k3t + k4t) / 6.0;
    w += h * (k1w + 2.0 * k2w + 2.0 * k3w + k4w) / 6.0;
  }

  out->x = x;
  out->v = v;
  out->th = th;
  out->w = w;
}
