void ref_traffic_second(const TrafficParams& p, TrafficRefState* st, const TrafficSecond& in, TrafficRefOut* out) {
    int entry_phase = st->phase;
    int entry_timer = st->timer;

    double pend = st->memory[0];
    bool pending = (pend > 0.5) || (in.ped_request != 0);

    double credits[4];
    credits[0] = st->memory[1];
    credits[1] = st->memory[2];
    credits[2] = st->memory[3];
    credits[3] = st->memory[4];

    int cur_phase = entry_phase;
    int cur_timer_before = entry_timer; // valid only if no transition yet
    bool transitioned_this_second = false;

    // all-red target length for current all-red phase
    int allred_len = p.all_red;
    bool allred_valid = (st->memory[6] > 0.5);
    if (cur_phase == 2 || cur_phase == 5) {
        if (allred_valid) allred_len = (int)st->memory[5];
        else allred_len = p.all_red;
    }

    // helper to get signals
    auto sig_of = [](int phase, int app) -> int {
        if (phase == 0) return (app == 0 || app == 2) ? 2 : 0;
        if (phase == 1) return (app == 0 || app == 2) ? 1 : 0;
        if (phase == 3) return (app == 1 || app == 3) ? 2 : 0;
        if (phase == 4) return (app == 1 || app == 3) ? 1 : 0;
        return 0;
    };

    // iterate possible chained transitions within same second
    for (int iter = 0; iter < 8; ++iter) {
        int elapsed_inc;
        if (!transitioned_this_second) elapsed_inc = cur_timer_before + 1;
        else elapsed_inc = 1; // phase began this second

        bool ends = false;
        if (cur_phase == 0 || cur_phase == 3) {
            bool isNS = (cur_phase == 0);
            int D0 = isNS ? 0 : 1;
            int D1 = isNS ? 2 : 3;
            int O0 = isNS ? 1 : 0;
            int O1 = isNS ? 3 : 2;
            bool emergD = (in.emergency[D0] != 0) || (in.emergency[D1] != 0);
            bool emergO = (in.emergency[O0] != 0) || (in.emergency[O1] != 0);
            if (emergD) {
                ends = false;
            } else if (emergO) {
                ends = true;
            } else if (p.actuated == 0) {
                int need = isNS ? p.green_ns : p.green_ew;
                if (elapsed_inc >= need) ends = true;
            } else {
                // actuated
                if (elapsed_inc >= p.max_green) {
                    ends = true;
                } else {
                    int need = isNS ? p.green_ns : p.green_ew;
                    if (elapsed_inc >= need) {
                        // visible counts only non-failed detectors, queue>0
                        bool visD = false;
                        bool visO = false;
                        // D approaches
                        int Ds[2] = {D0, D1};
                        int Os[2] = {O0, O1};
                        for (int k = 0; k < 2; ++k) {
                            int a = Ds[k];
                            if (in.detector_failed[a] == 0 && in.queue[a] > 0) visD = true;
                        }
                        for (int k = 0; k < 2; ++k) {
                            int a = Os[k];
                            if (in.detector_failed[a] == 0 && in.queue[a] > 0) visO = true;
                        }
                        bool recalled = false;
                        for (int k = 0; k < 2; ++k) {
                            int a = Os[k];
                            if (in.wait[a] >= 100) recalled = true;
                        }
                        bool condA = (!visD) && (visO || recalled);
                        bool condB = recalled;
                        if (condA || condB) ends = true;
                        else ends = false;
                    } else {
                        ends = false;
                    }
                }
            }
        } else if (cur_phase == 1 || cur_phase == 4) {
            if (elapsed_inc >= p.yellow) ends = true;
            else ends = false;
        } else { // 2 or 5 all-red
            if (elapsed_inc >= allred_len) ends = true;
            else ends = false;
        }

        if (!ends) break;

        int next_phase = (cur_phase + 1) % 6;
        // entering all-red: serve pending
        if (next_phase == 2 || next_phase == 5) {
            if (pending) {
                allred_len = p.all_red + 8;
                pending = false;
            } else {
                allred_len = p.all_red;
            }
            allred_valid = true;
        }
        cur_phase = next_phase;
        transitioned_this_second = true;
        // continue loop to check if new phase also ends this same second
    }

    int final_phase = cur_phase;

    // discharge using final phase signals
    int q[4];
    q[0] = in.queue[0]; q[1] = in.queue[1]; q[2] = in.queue[2]; q[3] = in.queue[3];
    double new_credits[4];
    for (int a = 0; a < 4; ++a) {
        int s = sig_of(final_phase, a);
        double c = credits[a];
        if (s != 2) {
            c = 0.0;
        } else {
            if (q[a] <= 0) {
                // keep credit
            } else {
                c += 0.5;
                while (c >= 1.0 - 1e-12 && q[a] > 0) {
                    q[a] -= 1;
                    c -= 1.0;
                    if (c < 0) c = 0;
                    // at most one per second given 0.5 increment, but loop safe
                    // prevent removing more than one per second: break after one?
                    // spec: each full credit removes one vehicle; with 0.5 gain max one
                    break;
                }
                if (q[a] < 0) q[a] = 0;
            }
        }
        new_credits[a] = c;
    }

    // red-wait update after discharge
    int w[4];
    for (int a = 0; a < 4; ++a) {
        int s = sig_of(final_phase, a);
        if (s == 0 && q[a] > 0) w[a] = in.wait[a] + 1;
        else w[a] = 0;
    }

    out->phase = final_phase;
    out->queue[0] = q[0]; out->queue[1] = q[1]; out->queue[2] = q[2]; out->queue[3] = q[3];
    out->wait[0] = w[0]; out->wait[1] = w[1]; out->wait[2] = w[2]; out->wait[3] = w[3];

    // update state
    st->phase = final_phase;
    if (transitioned_this_second) st->timer = 0;
    else st->timer = entry_timer + 1;

    st->memory[0] = pending ? 1.0 : 0.0;
    st->memory[1] = new_credits[0];
    st->memory[2] = new_credits[1];
    st->memory[3] = new_credits[2];
    st->memory[4] = new_credits[3];
    if (final_phase == 2 || final_phase == 5) {
        st->memory[5] = (double)allred_len;
        st->memory[6] = 1.0;
    } else {
        // keep invalid marker? clear validity
        st->memory[6] = 0.0;
    }
}
