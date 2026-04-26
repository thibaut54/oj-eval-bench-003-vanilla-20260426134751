#include <bits/stdc++.h>
using namespace std;

static const int M_MAX = 26;
static const int S_MAX = 4;

static const char* STATUS_NAMES[S_MAX] = {
    "Accepted", "Wrong_Answer", "Runtime_Error", "Time_Limit_Exceed"
};

static int statusIdx(const string& s) {
    for (int i = 0; i < S_MAX; ++i) {
        if (s == STATUS_NAMES[i]) return i;
    }
    return -1;
}

struct Submission {
    int problem = 0;
    int status = 0;
    int time = 0;
    bool valid = false;
};

struct ProbState {
    bool solved = false;
    int ac_time = 0;
    int wa_before_ac = 0;
    int wa_count = 0;
    bool frozen = false;
    int frozen_pre_wa = 0;
    int frozen_subs = 0;
};

struct Team {
    string name;
    int idx = 0;
    array<ProbState, M_MAX> probs{};
    int solved_count = 0;
    long long penalty = 0;
    vector<int> solve_times_desc;
    Submission last_overall;
    array<Submission, M_MAX> last_by_problem{};
    array<Submission, S_MAX> last_by_status{};
    array<array<Submission, S_MAX>, M_MAX> last_by_p_s{};
    int frozen_count = 0;
    int last_flush_rank = 0;
};

static int M = 0;
static int duration_time = 0;
static bool started = false;
static bool frozen_global = false;

static unordered_map<string, Team*> teams_by_name;
static vector<Team*> all_teams;

struct RankCmp {
    bool operator()(Team* a, Team* b) const {
        if (a->solved_count != b->solved_count) return a->solved_count > b->solved_count;
        if (a->penalty != b->penalty) return a->penalty < b->penalty;
        if (a->solve_times_desc != b->solve_times_desc) return a->solve_times_desc < b->solve_times_desc;
        return a->name < b->name;
    }
};

static set<Team*, RankCmp> board;
static set<Team*, RankCmp> frozen_board;

static string output_buf;
static char numbuf[32];

static inline void appendNum(int x) {
    int n = sprintf(numbuf, "%d", x);
    output_buf.append(numbuf, n);
}
static inline void appendNum(long long x) {
    int n = sprintf(numbuf, "%lld", x);
    output_buf.append(numbuf, n);
}

static void appendProbDisplay(const ProbState& ps) {
    output_buf += ' ';
    if (ps.frozen) {
        if (ps.frozen_pre_wa == 0) {
            output_buf += "0/";
            appendNum(ps.frozen_subs);
        } else {
            output_buf += '-';
            appendNum(ps.frozen_pre_wa);
            output_buf += '/';
            appendNum(ps.frozen_subs);
        }
    } else if (ps.solved) {
        output_buf += '+';
        if (ps.wa_before_ac > 0) appendNum(ps.wa_before_ac);
    } else {
        if (ps.wa_count == 0) output_buf += '.';
        else {
            output_buf += '-';
            appendNum(ps.wa_count);
        }
    }
}

static void outputScoreboardAndAssignRanks() {
    int rank = 0;
    for (auto t : board) {
        ++rank;
        t->last_flush_rank = rank;
        output_buf.append(t->name);
        output_buf += ' ';
        appendNum(rank);
        output_buf += ' ';
        appendNum(t->solved_count);
        output_buf += ' ';
        appendNum(t->penalty);
        for (int p = 0; p < M; ++p) {
            appendProbDisplay(t->probs[p]);
        }
        output_buf += '\n';
    }
}

static void assignRanksOnly() {
    int rank = 0;
    for (auto t : board) {
        ++rank;
        t->last_flush_rank = rank;
    }
}

static void insert_solve_time_desc(vector<int>& v, int t) {
    auto it = upper_bound(v.begin(), v.end(), t, greater<int>());
    v.insert(it, t);
}

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(nullptr);
    output_buf.reserve(1 << 22);

    string line;
    while (getline(cin, line)) {
        if (line.empty()) continue;
        // Strip trailing CR (Windows line endings)
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line.empty()) continue;

        size_t pos = 0;
        // Read first token
        size_t sp = line.find(' ', pos);
        string cmd = line.substr(pos, sp == string::npos ? string::npos : sp - pos);

        if (cmd == "ADDTEAM") {
            string name = line.substr(sp + 1);
            if (started) {
                output_buf += "[Error]Add failed: competition has started.\n";
            } else if (teams_by_name.count(name)) {
                output_buf += "[Error]Add failed: duplicated team name.\n";
            } else {
                Team* t = new Team();
                t->name = name;
                t->idx = (int)all_teams.size();
                teams_by_name[name] = t;
                all_teams.push_back(t);
                board.insert(t);
                output_buf += "[Info]Add successfully.\n";
            }
        } else if (cmd == "START") {
            // START DURATION x PROBLEM y
            istringstream iss(line);
            string a, b, c;
            int dur, m_val;
            iss >> a >> b >> dur >> c >> m_val;
            if (started) {
                output_buf += "[Error]Start failed: competition has started.\n";
            } else {
                duration_time = dur;
                M = m_val;
                started = true;
                assignRanksOnly();
                output_buf += "[Info]Competition starts.\n";
            }
        } else if (cmd == "SUBMIT") {
            // SUBMIT prob BY team WITH status AT time
            istringstream iss(line);
            string a, prob_s, byTok, teamName, withTok, statusS, atTok;
            int time_;
            iss >> a >> prob_s >> byTok >> teamName >> withTok >> statusS >> atTok >> time_;
            int p = prob_s[0] - 'A';
            Team* t = teams_by_name[teamName];
            int s = statusIdx(statusS);

            Submission sub;
            sub.problem = p;
            sub.status = s;
            sub.time = time_;
            sub.valid = true;

            t->last_overall = sub;
            t->last_by_problem[p] = sub;
            t->last_by_status[s] = sub;
            t->last_by_p_s[p][s] = sub;

            ProbState& ps = t->probs[p];

            // Possibly enter frozen state for this problem
            if (frozen_global && !ps.frozen && !ps.solved) {
                ps.frozen = true;
                ps.frozen_pre_wa = ps.wa_count;
                ps.frozen_subs = 0;
                if (t->frozen_count == 0) {
                    frozen_board.insert(t);
                }
                t->frozen_count++;
            }
            if (ps.frozen) {
                ps.frozen_subs++;
            }

            if (!ps.solved) {
                if (s == 0) { // Accepted
                    ps.solved = true;
                    ps.ac_time = time_;
                    ps.wa_before_ac = ps.wa_count;
                    if (!ps.frozen) {
                        // Update key: remove and reinsert
                        bool was_in_frozen = (t->frozen_count > 0);
                        board.erase(t);
                        if (was_in_frozen) frozen_board.erase(t);
                        t->solved_count++;
                        t->penalty += 20LL * ps.wa_before_ac + ps.ac_time;
                        insert_solve_time_desc(t->solve_times_desc, ps.ac_time);
                        board.insert(t);
                        if (was_in_frozen) frozen_board.insert(t);
                    }
                } else {
                    ps.wa_count++;
                }
            }
        } else if (cmd == "FLUSH") {
            assignRanksOnly();
            output_buf += "[Info]Flush scoreboard.\n";
        } else if (cmd == "FREEZE") {
            if (frozen_global) {
                output_buf += "[Error]Freeze failed: scoreboard has been frozen.\n";
            } else {
                frozen_global = true;
                output_buf += "[Info]Freeze scoreboard.\n";
            }
        } else if (cmd == "SCROLL") {
            if (!frozen_global) {
                output_buf += "[Error]Scroll failed: scoreboard has not been frozen.\n";
            } else {
                output_buf += "[Info]Scroll scoreboard.\n";
                // Pre-scroll scoreboard (this is post-flush)
                outputScoreboardAndAssignRanks();

                // Process unfreezes
                while (!frozen_board.empty()) {
                    auto rit = prev(frozen_board.end());
                    Team* t = *rit;
                    int p = -1;
                    for (int i = 0; i < M; ++i) {
                        if (t->probs[i].frozen) { p = i; break; }
                    }
                    if (p < 0) {
                        frozen_board.erase(t);
                        continue;
                    }
                    ProbState& ps = t->probs[p];

                    auto board_it = board.find(t);
                    Team* old_pred = (board_it == board.begin()) ? nullptr : *prev(board_it);

                    bool key_changes = ps.solved;

                    board.erase(board_it);
                    frozen_board.erase(t);

                    ps.frozen = false;
                    t->frozen_count--;
                    if (ps.solved) {
                        t->solved_count++;
                        t->penalty += 20LL * ps.wa_before_ac + ps.ac_time;
                        insert_solve_time_desc(t->solve_times_desc, ps.ac_time);
                    }

                    auto ins = board.insert(t);
                    auto new_it = ins.first;
                    if (t->frozen_count > 0) frozen_board.insert(t);

                    if (key_changes) {
                        Team* new_pred = (new_it == board.begin()) ? nullptr : *prev(new_it);
                        if (new_pred != old_pred) {
                            auto nxt = next(new_it);
                            if (nxt != board.end()) {
                                Team* t2 = *nxt;
                                output_buf.append(t->name);
                                output_buf += ' ';
                                output_buf.append(t2->name);
                                output_buf += ' ';
                                appendNum(t->solved_count);
                                output_buf += ' ';
                                appendNum(t->penalty);
                                output_buf += '\n';
                            }
                        }
                    }
                }

                outputScoreboardAndAssignRanks();
                frozen_global = false;
            }
        } else if (cmd == "QUERY_RANKING") {
            string name = line.substr(sp + 1);
            auto it = teams_by_name.find(name);
            if (it == teams_by_name.end()) {
                output_buf += "[Error]Query ranking failed: cannot find the team.\n";
            } else {
                Team* t = it->second;
                output_buf += "[Info]Complete query ranking.\n";
                if (frozen_global) {
                    output_buf += "[Warning]Scoreboard is frozen. The ranking may be inaccurate until it were scrolled.\n";
                }
                output_buf.append(name);
                output_buf += " NOW AT RANKING ";
                appendNum(t->last_flush_rank);
                output_buf += '\n';
            }
        } else if (cmd == "QUERY_SUBMISSION") {
            // QUERY_SUBMISSION team WHERE PROBLEM=x AND STATUS=y
            istringstream iss(line);
            string a, name, whereTok, probTok, andTok, statusTok;
            iss >> a >> name >> whereTok >> probTok >> andTok >> statusTok;
            string probVal = probTok.substr(8);   // "PROBLEM=" length = 8
            string statusVal = statusTok.substr(7); // "STATUS=" length = 7

            auto tit = teams_by_name.find(name);
            if (tit == teams_by_name.end()) {
                output_buf += "[Error]Query submission failed: cannot find the team.\n";
            } else {
                Team* t = tit->second;
                output_buf += "[Info]Complete query submission.\n";
                Submission* sub = nullptr;
                if (probVal == "ALL" && statusVal == "ALL") {
                    sub = &t->last_overall;
                } else if (probVal == "ALL") {
                    int si = statusIdx(statusVal);
                    sub = &t->last_by_status[si];
                } else if (statusVal == "ALL") {
                    int pi = probVal[0] - 'A';
                    sub = &t->last_by_problem[pi];
                } else {
                    int pi = probVal[0] - 'A';
                    int si = statusIdx(statusVal);
                    sub = &t->last_by_p_s[pi][si];
                }
                if (!sub->valid) {
                    output_buf += "Cannot find any submission.\n";
                } else {
                    output_buf.append(name);
                    output_buf += ' ';
                    output_buf += (char)('A' + sub->problem);
                    output_buf += ' ';
                    output_buf.append(STATUS_NAMES[sub->status]);
                    output_buf += ' ';
                    appendNum(sub->time);
                    output_buf += '\n';
                }
            }
        } else if (cmd == "END") {
            output_buf += "[Info]Competition ends.\n";
            break;
        }
    }

    fwrite(output_buf.data(), 1, output_buf.size(), stdout);
    return 0;
}
