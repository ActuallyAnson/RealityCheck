// Interface of the independent reference for the traffic-signal domain (shown to the model verbatim, and compiled into the runner).
// Phase codes: 0 NS green, 1 NS yellow, 2 all-red after NS, 3 EW green, 4 EW yellow, 5 all-red after EW. Approaches: 0 N, 1 E, 2 S, 3 W. Signal codes: 0 red, 1 yellow, 2 green.
struct TrafficParams { int green_ns, green_ew, yellow, all_red, max_green; int actuated; };  // seconds; actuated is 0 or 1
struct TrafficRefState {
  int phase;           // phase before this second (set by the caller before every call)
  int timer;           // whole seconds already spent in this phase, not counting the second in which it began (0 right after it began); set by the caller
  double memory[16];   // any memory you need across seconds (for example a pending pedestrian request); zero at the start of a run, kept between calls
};
struct TrafficSecond {
  int t;               // this second, counted from 1
  int queue[4];        // vehicles queued per approach after this second's arrivals, before any discharge
  int wait[4];         // seconds each approach has been red with vehicles queued, as of the end of the previous second
  int emergency[4];    // 1 if an emergency vehicle is on that approach this second
  int detector_failed[4];  // 1 if that approach's detector has failed this second
  int ped_request;     // 1 if a pedestrian request is made this second
};
struct TrafficRefOut { int phase; int queue[4]; int wait[4]; };  // phase, queues and red-waits at the end of this second
void ref_traffic_second(const TrafficParams& p, TrafficRefState* st, const TrafficSecond& in, TrafficRefOut* out);
