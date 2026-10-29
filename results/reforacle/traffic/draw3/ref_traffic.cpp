static int ref_sig_of(int phase, int ap) {
    if (phase == 0) {
        return (ap == 0 || ap == 2) ? 2 : 0;
    } else if (phase == 3) {
        return (ap == 1 || ap == 3) ? 2 : 0;
    } else if (phase == 1) {
        return (ap == 0 || ap == 2) ? 1 : 0;
    } else if (phase == 4) {
        return (ap == 1 || ap == 3) ? 1 : 0;
    } else {
        return 0;
    }
}

void ref_traffic_second(const TrafficParams& p, TrafficRefState* st, const TrafficSecond& in, TrafficRefOut* out) {
    int cur_phase = st->phase;
    int cur_timer = st->timer;

    double ped_pending = st->memory[0];
    if (in.ped_request) ped_pending = 1.0;

    int elapsed = cur_timer + 1;

    int next_phase = cur_phase;
    bool trans = false;

    if (cur_phase == 0 || cur_phase == 3) {
        bool isNS = (cur_phase == 0);
        int green_sec = isNS ? p.green_ns : p.green_ew;
        int a0 = isNS ? 0 : 1;
        int a1 = isNS ? 2 : 3;
        int b0 = isNS ? 1 : 0;
        int b1 = isNS ? 3 : 2;
        bool emergD = (in.emergency[a0] != 0) || (in.emergency[a1] != 0);
        bool emergO = (in.emergency[b0] != 0) || (in.emergency[b1] != 0);
        if (emergD) {
            trans = false;
        } else if (emergO) {
            trans = true;
        } else if (p.actuated == 0) {
            if (elapsed >= green_sec) trans = true;
        } else {
            if (elapsed >= p.max_green) {
                trans = true;
            } else if (elapsed >= green_sec) {
                bool visD = false;
                bool visO = false;
                if (in.detector_failed[a0] == 0 && in.queue[a0] > 0) visD = true;
                if (in.detector_failed[a1] == 0 && in.queue[a1] > 0) visD = true;
                if (in.detector_failed[b0] == 0 && in.queue[b0] > 0) visO = true;
                if (in.detector_failed[b1] == 0 && in.queue[b1] > 0) visO = true;
                bool recalledO = (in.wait[b0] >= 100) || (in.wait[b1] >= 100);
                bool cond = false;
                if (recalledO) cond = true;
                else if ((!visD) && visO) cond = true;
                if (cond) trans = true;
            } else {
                trans = false;
            }
        }
        if (trans) {
            next_phase = isNS ? 1 : 4;
        }
    } else if (cur_phase == 1 || cur_phase == 4) {
        if (elapsed >= p.yellow) {
            trans = true;
            next_phase = (cur_phase == 1) ? 2 : 5;
        }
    } else if (cur_phase == 2 || cur_phase == 5) {
        double ar_len = st->memory[5];
        if (ar_len < 0.5) {
            ar_len = (double)p.all_red;
        }
        if (elapsed >= (int)ar_len) {
            trans = true;
            next_phase = (cur_phase == 2) ? 3 : 0;
        } else {
            next_phase = cur_phase;
        }
    } else {
        next_phase = cur_phase;
    }

    int out_phase = trans ? next_phase : cur_phase;
    int out_timer = trans ? 0 : elapsed;

    if (trans && (out_phase == 2 || out_phase == 5)) {
        double len;
        if (ped_pending >= 0.5) {
            len = (double)(p.all_red + 8);
            ped_pending = 0.0;
        } else {
            len = (double)p.all_red;
        }
        st->memory[5] = len;
    }

    st->memory[0] = ped_pending;
    st->phase = out_phase;
    st->timer = out_timer;

    int q[4];
    q[0] = in.queue[0]; q[1] = in.queue[1]; q[2] = in.queue[2]; q[3] = in.queue[3];

    for (int i = 0; i < 4; ++i) {
        int sig = ref_sig_of(out_phase, i);
        double cred = st->memory[1 + i];
        if (sig != 2) {
            cred = 0.0;
        } else {
            if (q[i] > 0) {
                cred += 0.5;
                if (cred >= 1.0) {
                    cred -= 1.0;
                    q[i] -= 1;
                    if (q[i] < 0) q[i] = 0;
                }
            } else {
                // keep credit
            }
        }
        st->memory[1 + i] = cred;
    }

    int w[4];
    for (int i = 0; i < 4; ++i) {
        int sig = ref_sig_of(out_phase, i);
        if (sig == 0 && q[i] > 0) {
            w[i] = in.wait[i] + 1;
        } else {
            w[i] = 0;
        }
    }

    out->phase = out_phase;
    out->queue[0] = q[0]; out->queue[1] = q[1]; out->queue[2] = q[2]; out->queue[3] = q[3];
    out->wait[0] = w[0]; out->wait[1] = w[1]; out->wait[2] = w[2]; out->wait[3] = w[3];
}
