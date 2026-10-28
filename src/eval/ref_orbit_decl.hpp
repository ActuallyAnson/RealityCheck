// Interface of the independent reference step for the orbit domain (shown to the model verbatim, and compiled into the runner).
struct OrbitIn {
  double dt, G;   // outer step length and gravitational constant
  int n;          // number of bodies (2 to 6)
  double m[8];    // masses
  double x[8], y[8], vx[8], vy[8];  // state at the start of the step
};
struct OrbitOut { double x[8], y[8], vx[8], vy[8]; };  // state one outer step (time dt) later
void ref_step_orbit(const OrbitIn& in, OrbitOut* out);
