void ref_step_orbit(const OrbitIn& in, OrbitOut* out) {
    const double eps2 = 0.3 * 0.3;
    const double G = in.G;
    const int n = in.n;
    const int D = n * 4;
    // state y: per body [x, y, vx, vy]
    double y[24];
    double mm[8];
    for (int i = 0; i < n; ++i) {
        y[i*4+0] = in.x[i];
        y[i*4+1] = in.y[i];
        y[i*4+2] = in.vx[i];
        y[i*4+3] = in.vy[i];
        mm[i] = in.m[i];
    }
    auto deriv = [&](const double* s, double* ds) {
        // velocities
        for (int i = 0; i < n; ++i) {
            ds[i*4+0] = s[i*4+2];
            ds[i*4+1] = s[i*4+3];
            ds[i*4+2] = 0.0;
            ds[i*4+3] = 0.0;
        }
        // accelerations
        for (int i = 0; i < n; ++i) {
            double xi = s[i*4+0];
            double yi = s[i*4+1];
            double axi = 0.0, ayi = 0.0;
            for (int j = 0; j < n; ++j) {
                if (j == i) continue;
                double dx = s[j*4+0] - xi;
                double dy = s[j*4+1] - yi;
                double r2 = dx*dx + dy*dy + eps2;
                double inv = 1.0 / (r2 * std::sqrt(r2));
                double f = G * mm[j] * inv;
                axi += f * dx;
                ayi += f * dy;
            }
            ds[i*4+2] = axi;
            ds[i*4+3] = ayi;
        }
    };

    const double dt = in.dt;
    // Dormand-Prince 5(4) coefficients
    const double c2=1.0/5.0, c3=3.0/10.0, c4=4.0/5.0, c5=8.0/9.0, c6=1.0, c7=1.0;
    const double a21=1.0/5.0;
    const double a31=3.0/40.0, a32=9.0/40.0;
    const double a41=44.0/45.0, a42=-56.0/15.0, a43=32.0/9.0;
    const double a51=19372.0/6561.0, a52=-25360.0/2187.0, a53=64448.0/6561.0, a54=-212.0/729.0;
    const double a61=9017.0/3168.0, a62=-355.0/33.0, a63=46732.0/5247.0, a64=49.0/176.0, a65=-5103.0/18656.0;
    const double a71=35.0/384.0, a72=0.0, a73=500.0/1113.0, a74=125.0/192.0, a75=-2187.0/6784.0, a76=11.0/84.0;
    const double e1=35.0/384.0-5179.0/57600.0;
    const double e3=500.0/1113.0-7571.0/16695.0;
    const double e4=125.0/192.0-393.0/640.0;
    const double e5=-2187.0/6784.0+92097.0/339200.0;
    const double e6=11.0/84.0-187.0/2100.0;
    const double e7=-1.0/40.0;

    const double atol = 1e-13;
    const double rtol = 1e-13;

    double t = 0.0;
    double h = dt / 8.0;
    if (h <= 0) h = 1e-4;
    if (h > dt) h = dt;

    double k1[24], k2[24], k3[24], k4[24], k5[24], k6[24], k7[24];
    double tmp[24], y5[24], errv[24];

    deriv(y, k1);

    int iters = 0;
    while (t < dt && iters < 200000) {
        ++iters;
        if (t + h > dt) h = dt - t;
        if (h < 1e-15) { h = dt - t; }
        // k2
        for (int i = 0; i < D; ++i) tmp[i] = y[i] + h * (a21 * k1[i]);
        deriv(tmp, k2);
        // k3
        for (int i = 0; i < D; ++i) tmp[i] = y[i] + h * (a31*k1[i] + a32*k2[i]);
        deriv(tmp, k3);
        // k4
        for (int i = 0; i < D; ++i) tmp[i] = y[i] + h * (a41*k1[i] + a42*k2[i] + a43*k3[i]);
        deriv(tmp, k4);
        // k5
        for (int i = 0; i < D; ++i) tmp[i] = y[i] + h * (a51*k1[i] + a52*k2[i] + a53*k3[i] + a54*k4[i]);
        deriv(tmp, k5);
        // k6
        for (int i = 0; i < D; ++i) tmp[i] = y[i] + h * (a61*k1[i] + a62*k2[i] + a63*k3[i] + a64*k4[i] + a65*k5[i]);
        deriv(tmp, k6);
        // y5
        for (int i = 0; i < D; ++i) y5[i] = y[i] + h * (a71*k1[i] + a72*k2[i] + a73*k3[i] + a74*k4[i] + a75*k5[i] + a76*k6[i]);
        // k7
        deriv(y5, k7);
        // error
        for (int i = 0; i < D; ++i) errv[i] = h * (e1*k1[i] + e3*k3[i] + e4*k4[i] + e5*k5[i] + e6*k6[i] + e7*k7[i]);
        double sse = 0.0;
        for (int i = 0; i < D; ++i) {
            double sc = atol + rtol * std::max(std::fabs(y[i]), std::fabs(y5[i]));
            double e = errv[i] / sc;
            sse += e * e;
        }
        double err = std::sqrt(sse / (double)D);
        double fac;
        if (err == 0.0) fac = 5.0;
        else fac = 0.9 * std::pow(err, -0.2);
        if (fac < 0.2) fac = 0.2;
        if (fac > 5.0) fac = 5.0;
        if (err <= 1.0) {
            // accept
            for (int i = 0; i < D; ++i) y[i] = y5[i];
            t += h;
            // FSAL reuse: k1 = k7
            for (int i = 0; i < D; ++i) k1[i] = k7[i];
            if (dt - t < 1e-15) break;
            h = h * fac;
            if (t + h > dt) h = dt - t;
            // avoid tiny/huge
            if (h < 1e-14) h = 1e-14;
        } else {
            h = h * fac;
            if (h < 1e-14) h = 1e-14;
            // recompute k1 not needed (same y)
        }
    }

    for (int i = 0; i < n; ++i) {
        out->x[i] = y[i*4+0];
        out->y[i] = y[i*4+1];
        out->vx[i] = y[i*4+2];
        out->vy[i] = y[i*4+3];
    }
    for (int i = n; i < 8; ++i) {
        out->x[i] = 0; out->y[i] = 0; out->vx[i] = 0; out->vy[i] = 0;
    }
}
