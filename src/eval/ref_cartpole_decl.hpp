// Interface of the independent reference step for the cart-pole domain (shown to the model verbatim, and compiled into the runner).
struct CartIn {
  double dt;                 // outer step length
  double mc, mp, l, g;       // cart mass, pole mass, pole length, gravity
  double u;                  // horizontal force applied to the cart, constant during the step (positive pushes the cart toward +x)
  double x, v, th, w;        // state at the start of the step: cart position, cart velocity, pole angle from upright, pole angular rate
};
struct CartOut { double x, v, th, w; };  // state one outer step (time dt) later
void ref_step_cartpole(const CartIn& in, CartOut* out);
