void ref_step_cartpole(const CartIn& in, CartOut* out) {
  double mc = in.mc;
  double mp = in.mp;
  double l = in.l;
  double g = in.g;
  double u = in.u;
  double M = mc + mp;

  auto accel = [&](double th, double w, double& a, double& al) {
    double s = std::sin(th);
    double c = std::cos(th);
    double den = mc + mp * s * s;
    double common_w = mp * l * s * w * w;
    a = (u + common_w - mp * g * s * c) / den;
    al = (M * g * s - u * c - common_w * c) / (l * den);
  };

  double x = in.x;
  double v = in.v;
  double th = in.th;
  double w = in.w;
  double dt = in.dt;

  int N = (int)std::ceil(dt / 0.0001);
  if (N < 20) N = 20;
  if (N > 400) N = 400;
  double h = dt / (double)N;

  for (int i = 0; i < N; ++i) {
    double a1, al1, a2, al2, a3, al3, a4, al4;

    double k1x = v;
    accel(th, w, a1, al1);
    double k1v = a1;
    double k1t = w;
    double k1w = al1;

    double v2 = v + 0.5 * h * k1v;
    double w2 = w + 0.5 * h * k1w;
    double th2 = th + 0.5 * h * k1t;
    double k2x = v2;
    accel(th2, w2, a2, al2);
    double k2v = a2;
    double k2t = w2;
    double k2w = al2;

    double v3 = v + 0.5 * h * k2v;
    double w3 = w + 0.5 * h * k2w;
    double th3 = th + 0.5 * h * k2t;
    double k3x = v3;
    accel(th3, w3, a3, al3);
    double k3v = a3;
    double k3t = w3;
    double k3w = al3;

    double v4 = v + h * k3v;
    double w4 = w + h * k3w;
    double th4 = th + h * k3t;
    double k4x = v4;
    accel(th4, w4, a4, al4);
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
