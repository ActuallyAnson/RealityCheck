void ref_traffic_second(const TrafficParams& p, TrafficRefState* st, const TrafficSecond& in, TrafficRefOut* out) {
  int cur_phase = st->phase;
  int cur_timer = st->timer;
  double pending = st->memory[0];
  if (in.ped_request) pending = 1.0;

  int elapsed = cur_timer + 1;

  int next_phase = cur_phase;
  bool transition = false;

  auto isNS = [](int a) -> bool { return a == 0 || a == 2; };
  auto isEW = [](int a) -> bool { return a == 1 || a == 3; };

  if (cur_phase == 0 || cur_phase == 3) {
    bool isNSgreen = (cur_phase == 0);
    int green_need = isNSgreen ? p.green_ns : p.green_ew;
    bool emerD = false;
    bool emerO = false;
    for (int a = 0; a < 4; ++a) if (in.emergency[a]) {
      bool inD = isNSgreen ? isNS(a) : isEW(a);
      bool inO = isNSgreen ? isEW(a) : isNS(a);
      if (inD) emerD = true;
      if (inO) emerO = true;
    }
    bool end = false;
    if (emerD) {
      end = false;
    } else if (emerO) {
      end = true;
    } else if (p.actuated == 0) {
      if (elapsed >= green_need) end = true;
    } else {
      if (elapsed >= p.max_green) {
        end = true;
      } else if (elapsed >= green_need) {
        bool noVisD = true;
        bool visO = false;
        bool recalledO = false;
        for (int a = 0; a < 4; ++a) {
          bool inD = isNSgreen ? isNS(a) : isEW(a);
          bool inO = isNSgreen ? isEW(a) : isNS(a);
          if (inD) {
            if (!in.detector_failed[a] && in.queue[a] > 0) noVisD = false;
          }
          if (inO) {
            if (!in.detector_failed[a] && in.queue[a] > 0) visO = true;
            if (in.wait[a] >= 100) recalledO = true;
          }
        }
        if (recalledO || (noVisD && visO)) end = true;
      }
    }
    if (end) {
      next_phase = cur_phase + 1;
      transition = true;
    }
  } else if (cur_phase == 1 || cur_phase == 4) {
    if (elapsed >= p.yellow) {
      next_phase = cur_phase + 1;
      transition = true;
    }
  } else if (cur_phase == 2 || cur_phase == 5) {
    int len = 0;
    double stored = st->memory[1];
    if (stored > 0.5) len = (int)(stored + 0.5);
    else len = p.all_red;
    if (elapsed >= len) {
      next_phase = (cur_phase + 1) % 6;
      transition = true;
    }
  }

  if (transition && (next_phase == 2 || next_phase == 5)) {
    int len = (pending >= 0.5) ? (p.all_red + 8) : p.all_red;
    st->memory[1] = (double)len;
    pending = 0.0;
  }
  st->memory[0] = pending;

  int final_phase = next_phase;
  int next_timer = transition ? 0 : (cur_timer + 1);

  auto sig = [](int ph, int a) -> int {
    if (ph == 0) return (a == 0 || a == 2) ? 2 : 0;
    if (ph == 1) return (a == 0 || a == 2) ? 1 : 0;
    if (ph == 2) return 0;
    if (ph == 3) return (a == 1 || a == 3) ? 2 : 0;
    if (ph == 4) return (a == 1 || a == 3) ? 1 : 0;
    return 0;
  };

  int nq[4];
  for (int a = 0; a < 4; ++a) nq[a] = in.queue[a];

  for (int a = 0; a < 4; ++a) {
    int s = sig(final_phase, a);
    double &cred = st->memory[2 + a];
    if (s != 2) {
      cred = 0.0;
    } else {
      if (nq[a] > 0) {
        cred = cred + 0.5;
        while (cred >= 1.0 && nq[a] > 0) {
          cred = cred - 1.0;
          nq[a] = nq[a] - 1;
        }
        if (nq[a] == 0) {
        }
      } else {
      }
      if (cred < 0) cred = 0;
    }
  }

  int nw[4];
  for (int a = 0; a < 4; ++a) {
    int s = sig(final_phase, a);
    if (s == 0 && nq[a] > 0) nw[a] = in.wait[a] + 1;
    else nw[a] = 0;
  }

  st->phase = final_phase;
  st->timer = next_timer;

  out->phase = final_phase;
  for (int a = 0; a < 4; ++a) {
    out->queue[a] = nq[a];
    out->wait[a] = nw[a];
  }
}
