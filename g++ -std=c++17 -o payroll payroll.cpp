// Employee Attendance & Payroll System
// Build: g++ -std=c++17 -o payroll payroll.cpp
// Run:   ./payroll
//
// Salary rules (editable in Settings):
//   daily rate   = monthly salary / working days per month
//   deduction    = absent days x daily rate
//   overtime pay = overtime hours x hourly rate x overtime multiplier
//   net salary   = monthly salary - deduction + overtime pay
//
// Money is stored as integer cents (paisa) to avoid rounding errors.

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <cctype>

using namespace std;

struct Employee {
    string id, name, address, bank;   // bank = bank account number
    long long salary;                 // monthly salary in cents
};

struct Attendance {
    string empId, date;               // date = YYYY-MM-DD
    char status;                      // 'P' present, 'A' absent
    int otMinutes;                    // overtime in minutes
};

struct Payment {
    string ref, empId, month, bank, paidOn;
    long long amount;
};

struct Settings {
    int workingDays = 26;
    double hoursPerDay = 8;
    double otMultiplier = 1.5;
};

struct Payslip {
    int present = 0, absent = 0, otMinutes = 0;
    long long base = 0, daily = 0, deduction = 0, otPay = 0, net = 0;
};

// ---------- helpers ----------
string money(long long c) {
    ostringstream os;
    if (c < 0) { os << "-"; c = -c; }
    os << c / 100 << "." << setw(2) << setfill('0') << c % 100;
    return os.str();
}

bool parseMoney(const string& s, long long& out) {
    char* end;
    double v = strtod(s.c_str(), &end);
    if (end == s.c_str() || *end != '\0' || v <= 0) return false;
    out = llround(v * 100);
    return true;
}

bool validMonth(const string& m) {
    return m.size() == 7 && m[4] == '-' && isdigit(m[0]) && isdigit(m[1]) &&
           isdigit(m[2]) && isdigit(m[3]) && isdigit(m[5]) && isdigit(m[6]) &&
           stoi(m.substr(5, 2)) >= 1 && stoi(m.substr(5, 2)) <= 12;
}

bool validDate(const string& d) {
    return d.size() == 10 && d[7] == '-' && validMonth(d.substr(0, 7)) &&
           isdigit(d[8]) && isdigit(d[9]) && stoi(d.substr(8, 2)) >= 1 &&
           stoi(d.substr(8, 2)) <= 31;
}

string clean(string s, char bad = '|') {
    replace(s.begin(), s.end(), bad, ' ');
    return s;
}

string today() {
    time_t t = time(nullptr);
    char buf[16];
    strftime(buf, sizeof buf, "%Y-%m-%d", localtime(&t));
    return buf;
}

string prompt(const string& msg) {
    cout << msg;
    string s;
    getline(cin, s);
    return s;
}

vector<string> split(const string& line, char sep = '|') {
    vector<string> parts;
    stringstream ss(line);
    string tok;
    while (getline(ss, tok, sep)) parts.push_back(tok);
    return parts;
}

// ---------- main system ----------
class PayrollSystem {
    vector<Employee> employees;
    vector<Attendance> records;
    vector<Payment> payments;
    Settings cfg;

    const string empFile = "employees.txt";
    const string attFile = "attendance.txt";
    const string payFile = "payments.txt";
    const string cfgFile = "settings.txt";

    Employee* find(const string& id) {
        for (auto& e : employees) if (e.id == id) return &e;
        return nullptr;
    }

    bool alreadyPaid(const string& id, const string& month) const {
        for (const auto& p : payments)
            if (p.empId == id && p.month == month) return true;
        return false;
    }

    Payslip computeSlip(const Employee& e, const string& month) const {
        Payslip s;
        for (const auto& a : records) {
            if (a.empId != e.id || a.date.compare(0, 7, month) != 0) continue;
            if (a.status == 'P') { s.present++; s.otMinutes += a.otMinutes; }
            else s.absent++;
        }
        s.base = e.salary;
        s.daily = llround((double)e.salary / cfg.workingDays);
        s.deduction = min(e.salary, s.absent * s.daily);
        s.otPay = llround(s.otMinutes * (double)s.daily * cfg.otMultiplier /
                          (cfg.hoursPerDay * 60.0));
        s.net = s.base - s.deduction + s.otPay;
        return s;
    }

    void printSlip(const Employee& e, const string& month) const {
        Payslip s = computeSlip(e, month);
        cout << "\n========== PAYSLIP " << month << " ==========\n"
             << "Employee ID : " << e.id << "\n"
             << "Name        : " << e.name << "\n"
             << "Address     : " << e.address << "\n"
             << "Bank account: " << e.bank << "\n"
             << "---------------------------------------\n"
             << "Days present        : " << s.present << "\n"
             << "Days absent         : " << s.absent << "\n"
             << "Overtime            : " << fixed << setprecision(2)
             << s.otMinutes / 60.0 << " hours\n"
             << "---------------------------------------\n"
             << left << setw(24) << "Basic salary" << right << setw(14) << money(s.base) << "\n"
             << left << setw(24) << ("Daily rate") << right << setw(14) << money(s.daily) << "\n"
             << left << setw(24) << "Absence deduction (-)" << right << setw(14) << money(s.deduction) << "\n"
             << left << setw(24) << "Overtime pay (+)" << right << setw(14) << money(s.otPay) << "\n"
             << "---------------------------------------\n"
             << left << setw(24) << "NET SALARY" << right << setw(14) << money(s.net) << "\n"
             << "Status: " << (alreadyPaid(e.id, month) ? "PAID" : "NOT PAID YET") << "\n";
    }

    void setAttendance(const string& id, const string& date, char status, int otMin) {
        for (auto& a : records)
            if (a.empId == id && a.date == date) { a.status = status; a.otMinutes = otMin; return; }
        records.push_back({id, date, status, otMin});
    }

    // Reads an overtime value in hours; returns minutes, or -1 if invalid.
    static int askOvertime() {
        string s = prompt("  Overtime hours (Enter = 0): ");
        if (s.empty()) return 0;
        char* end;
        double h = strtod(s.c_str(), &end);
        if (*end != '\0' || h < 0 || h > 24) return -1;
        return (int)llround(h * 60);
    }

public:
    // ----- employees -----
    void addEmployee() {
        string id = prompt("Employee ID: ");
        if (id.empty() || id.find('|') != string::npos) { cout << "Invalid ID.\n"; return; }
        if (find(id)) { cout << "That ID already exists.\n"; return; }
        string name = prompt("Full name: ");
        string address = prompt("Address: ");
        string bank = prompt("Bank account number: ");
        long long salary;
        if (name.empty() || bank.empty() || !parseMoney(prompt("Monthly salary: "), salary)) {
            cout << "Invalid input. Employee not added.\n";
            return;
        }
        employees.push_back({id, clean(name), clean(address), clean(bank), salary});
        cout << "Employee added.\n";
    }

    void listEmployees() const {
        if (employees.empty()) { cout << "No employees yet.\n"; return; }
        cout << "\n" << left << setw(8) << "ID" << setw(22) << "Name" << setw(20) << "Address"
             << setw(16) << "Bank account" << right << setw(12) << "Salary" << "\n"
             << string(78, '-') << "\n";
        for (const auto& e : employees)
            cout << left << setw(8) << e.id << setw(22) << e.name.substr(0, 20)
                 << setw(20) << e.address.substr(0, 18) << setw(16) << e.bank
                 << right << setw(12) << money(e.salary) << "\n";
    }

    void removeEmployee() {
        string id = prompt("Employee ID to remove: ");
        auto it = remove_if(employees.begin(), employees.end(),
                            [&](const Employee& e) { return e.id == id; });
        if (it == employees.end()) { cout << "No such employee.\n"; return; }
        employees.erase(it, employees.end());
        cout << "Employee removed (their past attendance/payment records are kept).\n";
    }

    // ----- attendance -----
    void markOne() {
        Employee* e = find(prompt("Employee ID: "));
        if (!e) { cout << "No such employee.\n"; return; }
        string date = prompt("Date (YYYY-MM-DD, Enter = today): ");
        if (date.empty()) date = today();
        if (!validDate(date)) { cout << "Invalid date.\n"; return; }
        string st = prompt("Status (P = present, A = absent): ");
        if (st.empty() || (toupper(st[0]) != 'P' && toupper(st[0]) != 'A')) {
            cout << "Invalid status.\n";
            return;
        }
        char status = toupper(st[0]);
        int ot = 0;
        if (status == 'P') {
            ot = askOvertime();
            if (ot < 0) { cout << "Invalid overtime.\n"; return; }
        }
        setAttendance(e->id, date, status, ot);
        cout << "Attendance saved for " << e->name << " on " << date << ".\n";
    }

    void markAll() {
        if (employees.empty()) { cout << "No employees yet.\n"; return; }
        string date = prompt("Date (YYYY-MM-DD, Enter = today): ");
        if (date.empty()) date = today();
        if (!validDate(date)) { cout << "Invalid date.\n"; return; }
        for (const auto& e : employees) {
            cout << "\n" << e.id << " - " << e.name << "\n";
            string st = prompt("  Status (Enter = present, A = absent): ");
            char status = (!st.empty() && toupper(st[0]) == 'A') ? 'A' : 'P';
            int ot = 0;
            if (status == 'P') {
                ot = askOvertime();
                if (ot < 0) { cout << "  Invalid overtime, using 0.\n"; ot = 0; }
            }
            setAttendance(e.id, date, status, ot);
        }
        cout << "\nAttendance saved for " << date << ".\n";
    }

    void viewAttendance() {
        Employee* e = find(prompt("Employee ID: "));
        if (!e) { cout << "No such employee.\n"; return; }
        string month = prompt("Month (YYYY-MM): ");
        if (!validMonth(month)) { cout << "Invalid month.\n"; return; }
        vector<Attendance> list;
        for (const auto& a : records)
            if (a.empId == e->id && a.date.compare(0, 7, month) == 0) list.push_back(a);
        sort(list.begin(), list.end(),
             [](const Attendance& a, const Attendance& b) { return a.date < b.date; });
        cout << "\nAttendance of " << e->name << " for " << month << "\n"
             << left << setw(14) << "Date" << setw(10) << "Status" << "Overtime(h)\n"
             << string(36, '-') << "\n";
        for (const auto& a : list)
            cout << left << setw(14) << a.date << setw(10) << (a.status == 'P' ? "Present" : "Absent")
                 << fixed << setprecision(2) << a.otMinutes / 60.0 << "\n";
        if (list.empty()) cout << "(no records)\n";
    }

    // ----- salary -----
    void viewPayslip() {
        Employee* e = find(prompt("Employee ID: "));
        if (!e) { cout << "No such employee.\n"; return; }
        string month = prompt("Month (YYYY-MM): ");
        if (!validMonth(month)) { cout << "Invalid month.\n"; return; }
        printSlip(*e, month);
    }

    void payrollSummary() const {
        string month = prompt("Month (YYYY-MM): ");
        if (!validMonth(month)) { cout << "Invalid month.\n"; return; }
        long long total = 0;
        cout << "\nPayroll summary for " << month << "\n"
             << left << setw(8) << "ID" << setw(20) << "Name" << right << setw(5) << "Pres"
             << setw(5) << "Abs" << setw(7) << "OT(h)" << setw(12) << "Deduct"
             << setw(12) << "Overtime" << setw(13) << "Net" << "  Paid\n"
             << string(86, '-') << "\n";
        for (const auto& e : employees) {
            Payslip s = computeSlip(e, month);
            cout << left << setw(8) << e.id << setw(20) << e.name.substr(0, 18) << right
                 << setw(5) << s.present << setw(5) << s.absent << setw(7) << fixed
                 << setprecision(1) << s.otMinutes / 60.0 << setw(12) << money(s.deduction)
                 << setw(12) << money(s.otPay) << setw(13) << money(s.net)
                 << "  " << (alreadyPaid(e.id, month) ? "Yes" : "No") << "\n";
            total += s.net;
        }
        cout << string(86, '-') << "\n"
             << left << setw(57) << "TOTAL NET PAYROLL" << right << setw(13) << money(total) << "\n";
    }

    void paySalaries() {
        string month = prompt("Month to pay (YYYY-MM): ");
        if (!validMonth(month)) { cout << "Invalid month.\n"; return; }

        vector<pair<const Employee*, long long>> due;
        long long total = 0;
        for (const auto& e : employees) {
            if (alreadyPaid(e.id, month)) continue;
            long long net = computeSlip(e, month).net;
            if (net > 0) { due.push_back({&e, net}); total += net; }
        }
        if (due.empty()) { cout << "Nothing to pay for " << month << ".\n"; return; }

        cout << "\nSalaries to be paid for " << month << ":\n";
        for (const auto& d : due)
            cout << "  " << left << setw(8) << d.first->id << setw(22) << d.first->name
                 << setw(16) << d.first->bank << right << setw(12) << money(d.second) << "\n";
        cout << "TOTAL: " << money(total) << "  (" << due.size() << " employees)\n";

        if (prompt("Type YES to confirm payment: ") != "YES") { cout << "Cancelled.\n"; return; }

        string csvName = "bank_transfer_" + month + ".csv";
        ofstream csv(csvName);
        csv << "Reference,Account Number,Account Name,Amount,Narration\n";
        for (const auto& d : due) {
            const Employee& e = *d.first;
            string ref = "PAY-" + month + "-" + e.id;
            payments.push_back({ref, e.id, month, e.bank, today(), d.second});
            csv << ref << "," << clean(e.bank, ',') << "," << clean(e.name, ',') << ","
                << money(d.second) << ",Salary " << month << "\n";
        }
        cout << "\nPayments recorded. Bank transfer file created: " << csvName << "\n"
             << "Upload this file to your bank's bulk-payment portal to send the money.\n";
    }

    void settingsMenu() {
        cout << "\nCurrent settings:\n"
             << "  1. Working days per month : " << cfg.workingDays << "\n"
             << "  2. Working hours per day  : " << cfg.hoursPerDay << "\n"
             << "  3. Overtime multiplier    : " << cfg.otMultiplier << "x\n";
        string c = prompt("Change which (1-3, Enter = none): ");
        if (c.empty()) return;
        double v = atof(prompt("New value: ").c_str());
        if (c == "1" && v >= 1 && v <= 31) cfg.workingDays = (int)v;
        else if (c == "2" && v > 0 && v <= 24) cfg.hoursPerDay = v;
        else if (c == "3" && v >= 1) cfg.otMultiplier = v;
        else { cout << "Invalid.\n"; return; }
        cout << "Updated.\n";
    }

    // ----- persistence -----
    void save() const {
        ofstream f(empFile);
        for (const auto& e : employees)
            f << e.id << "|" << e.name << "|" << e.address << "|" << e.bank << "|" << e.salary << "\n";
        ofstream a(attFile);
        for (const auto& r : records)
            a << r.empId << "|" << r.date << "|" << r.status << "|" << r.otMinutes << "\n";
        ofstream p(payFile);
        for (const auto& x : payments)
            p << x.ref << "|" << x.empId << "|" << x.month << "|" << x.bank << "|"
              << x.paidOn << "|" << x.amount << "\n";
        ofstream c(cfgFile);
        c << cfg.workingDays << "|" << cfg.hoursPerDay << "|" << cfg.otMultiplier << "\n";
    }

    void load() {
        string line;
        try {
            ifstream f(empFile);
            while (getline(f, line)) {
                auto p = split(line);
                if (p.size() >= 5) employees.push_back({p[0], p[1], p[2], p[3], stoll(p[4])});
            }
            ifstream a(attFile);
            while (getline(a, line)) {
                auto p = split(line);
                if (p.size() >= 4) records.push_back({p[0], p[1], p[2][0], stoi(p[3])});
            }
            ifstream pf(payFile);
            while (getline(pf, line)) {
                auto p = split(line);
                if (p.size() >= 6) payments.push_back({p[0], p[1], p[2], p[3], p[4], stoll(p[5])});
            }
            ifstream c(cfgFile);
            if (getline(c, line)) {
                auto p = split(line);
                if (p.size() >= 3) {
                    cfg.workingDays = stoi(p[0]);
                    cfg.hoursPerDay = stod(p[1]);
                    cfg.otMultiplier = stod(p[2]);
                }
            }
        } catch (...) {
            cout << "Warning: a data file looked corrupted; some data may be missing.\n";
        }
    }
};

int main() {
    PayrollSystem sys;
    sys.load();

    while (true) {
        cout << "\n===== EMPLOYEE ATTENDANCE & PAYROLL =====\n"
             << " 1. Add employee\n"
             << " 2. List employees\n"
             << " 3. Remove employee\n"
             << " 4. Mark attendance (one employee)\n"
             << " 5. Mark attendance (all employees for a day)\n"
             << " 6. View employee attendance\n"
             << " 7. View salary slip\n"
             << " 8. Payroll summary (all employees)\n"
             << " 9. Pay salaries (record + bank transfer file)\n"
             << "10. Settings (working days, overtime rate)\n"
             << " 0. Exit\n";
        string choice = prompt("Choose: ");
        if (!cin) break;

        if      (choice == "1")  sys.addEmployee();
        else if (choice == "2")  sys.listEmployees();
        else if (choice == "3")  sys.removeEmployee();
        else if (choice == "4")  sys.markOne();
        else if (choice == "5")  sys.markAll();
        else if (choice == "6")  sys.viewAttendance();
        else if (choice == "7")  sys.viewPayslip();
        else if (choice == "8")  sys.payrollSummary();
        else if (choice == "9")  sys.paySalaries();
        else if (choice == "10") sys.settingsMenu();
        else if (choice == "0")  break;
        else cout << "Invalid choice.\n";

        sys.save();   // save after every action so nothing is lost
    }
    sys.save();
    cout << "Data saved. Goodbye!\n";
    return 0;
}
