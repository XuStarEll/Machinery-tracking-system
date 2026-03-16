// Kaizen Machinery Monitor – Real-Time Industrial Tracking System
// Author: Setareh Ehsani
// Version: Hybrid Pro Real-Time + History Logging
// Description: Tracks machine usage, provides predictive maintenance alerts, logs history.
// Inspired by Japanese Kaizen principles.

#include <iostream>
#include <vector>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <ctime>

using namespace std;
using namespace chrono;

#define RESET "\033[0m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define ORANGE "\033[38;5;208m"
#define CYAN "\033[36m"

//Represents a machine in the system with status, timing, and history
struct Machine {
    int id;                             // Unique machine ID
    string name;                        // Name of the machines
    int totalHours;                     // (legacy) total hours used manually - no longer used
    int limit;                          // Usage time limit (in minutes)
    string status;                      // Current status: Active, Warning, Under Repair
    bool isShutDown;                    // Flag to indicate if machine is shutdown
    time_point<steady_clock> startTime; // Start time of machine usage
    int pausedSeconds = 0;              // Time used before shutdown
    vector<string> history;             // List of status change history logs
};

vector<Machine> machines;

// Returns the current local system time as a formatted string
// Format: "YYYY-MM-DD HH:MM:SS"
// Used for timestamping machine history entries
string getTimeNow() {
    time_t now = time(nullptr);
    char buf[100];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    return string(buf);
}

// Adds a timestamped entry to a machine's history log
void logHistory(Machine &m, const string& statusChange) {
    m.history.push_back(getTimeNow() + " - " + statusChange);
}

// Updates machine status based on usage time
void updateStatus(Machine& m, int totalSeconds) {
    int limitSeconds = m.limit * 60;
    string oldStatus = m.status;
    if (totalSeconds >= limitSeconds)
        m.status = "Under Repair";
    else if (totalSeconds >= limitSeconds - 10)
        m.status = "Warning";
    else
        m.status = "Active";

    if (m.status != oldStatus) {
        logHistory(m, "Changed to " + m.status);
    }
}

// Registers a new machine and initializes its properties
void registerMachine() {
    Machine m;
    cout << "Enter machine ID: "; cin >> m.id;
    cout << "Enter machine name: "; cin.ignore(); getline(cin, m.name);
    cout << "Enter time limit (minutes): "; cin >> m.limit;
    m.totalHours = 0;
    m.status = "Active";
    m.isShutDown = false;
    m.startTime = steady_clock::now();
    m.pausedSeconds = 0;
    logHistory(m, "Registered (Active)");
    machines.push_back(m);
    cout << GREEN << "Machine registered successfully.\n" << RESET;
}

// Checks all machines for high usage and suggests maintence if needed
void predictiveMaintenanceCheck() {
    cout << CYAN << "\nPredictive Maintenance Check:\n" << RESET;
    ofstream alertFile("alerts.txt");
    auto now = steady_clock::now();
    bool anyWarning = false;
    for (auto& m : machines) {
        int totalSeconds = m.isShutDown ? m.pausedSeconds : duration_cast<seconds>(now - m.startTime).count();
        updateStatus(m, totalSeconds);
        int limitSeconds = m.limit * 60;
        float usage = (float)totalSeconds / limitSeconds * 100;
        if (usage >= 90 && usage < 100) {
            anyWarning = true;
            string message = "⚠  Machine " + m.name + " (ID: " + to_string(m.id) + ") is at " + to_string((int)usage) + "% usage – maintenance recommended.";
            cout << ORANGE << message << RESET << endl;
            alertFile << message << endl;
        }
    }
    if (!anyWarning) {
        cout << GREEN << "All machines are operating within safe usage limits.\n" << RESET;
        alertFile << "All machines are operating within safe usage limits." << endl;
    }
    alertFile.close();
}

// Displays all registered machines with real-time status and color
void showMachines() {
    if (machines.empty()) {
        cout << YELLOW << "No machines registered yet.\n" << RESET;
        return;
    }
    cout << CYAN << left << setw(5) << "ID" << setw(20) << "Name" << setw(15) << "Status" << setw(15) << "Usage Time" << setw(10) << "Limit" << setw(10) << "Shutdown" << RESET << endl;
    auto now = steady_clock::now();
    for (auto& m : machines) {    // Capture current time to compute usage
        int totalSeconds = m.isShutDown ? m.pausedSeconds : duration_cast<seconds>(now - m.startTime).count();
        updateStatus(m, totalSeconds);  // Ensure status is updated based on current time
        int minutes = totalSeconds / 60;
        int seconds = totalSeconds % 60;
        string color = (m.status == "Active") ? GREEN : (m.status == "Warning") ? ORANGE : RED;
        string usageTime = to_string(minutes) + "m " + to_string(seconds) + "s";
        cout << color << left << setw(5) << m.id << setw(20) << m.name << setw(15) << m.status
             << setw(15) << usageTime << setw(10) << m.limit << setw(10) << (m.isShutDown ? "Yes" : "No") << RESET << endl;
    }
}

// Resets a machine's status after maintence
void markRepaired() {
    int id;
    cout << "Enter ID of machine to mark as repaired: "; cin >> id;
    for (auto& m : machines) {
        if (m.id == id && (m.status == "Under Repair" || m.status == "Warning")) {
            m.status = "Active";
            m.totalHours = 0;
            m.isShutDown = false;
            m.startTime = steady_clock::now();
            m.pausedSeconds = 0;
            logHistory(m, "Repaired (Reset)");
            cout << GREEN << "[SUCCESS] Machine \"" << m.name << "\" (ID: " << m.id << ") has been repaired and reset.\n" << RESET;
            return;
        }
    }
    cout << RED << "Machine not found or not in repair/warning status.\n" << RESET;
}

// Shuts down a warning-state machine and pauses timer
void shutDownWarningMachine() {
    int id;
    cout << "Enter ID of machine to shut down (Warning only): "; cin >> id;
    auto now = steady_clock::now();
    for (auto& m : machines) {
        if (m.id == id && m.status == "Warning" && !m.isShutDown) {
            m.isShutDown = true;
            m.pausedSeconds = duration_cast<seconds>(now - m.startTime).count();
            logHistory(m, "Manually Shut Down");
            cout << ORANGE << "Machine " << m.name << " is now shut down until maintenance is done.\n" << RESET;
            return;
        }
    }
    cout << RED << "Machine not found or not in Warning status.\n" << RESET;
}

// Displays a dashboard summary and saves it to dashboard.txt
void showDashboard() {
    ofstream dashFile("dashboard.txt");
    cout << CYAN << "\n----------------- MACHINERY DASHBOARD ------------------" << RESET << endl;
    cout << left << setw(5) << "ID" << setw(18) << "Name" << setw(16) << "Status"
         << setw(10) << "Usage" << setw(10) << "Shutdown" << endl;
    cout << "------------------------------------------------------" << endl;
    dashFile << "MACHINERY DASHBOARD\n";
    dashFile << "ID   Name             Status         Usage   Shutdown\n";
    dashFile << "------------------------------------------------------\n";
    auto now = steady_clock::now();
    for (auto& m : machines) {
        int totalSeconds = m.isShutDown ? m.pausedSeconds : duration_cast<seconds>(now - m.startTime).count();
        updateStatus(m, totalSeconds);
        float usage = (float)totalSeconds / (m.limit * 60) * 100;
        string color = (m.status == "Active") ? GREEN : (m.status == "Warning") ? ORANGE : RED;
        cout << color << left << setw(5) << m.id << setw(18) << m.name << setw(16) << m.status
             << setw(9) << (int)usage << "%" << setw(10) << (m.isShutDown ? "Yes" : "No") << RESET << endl;
        dashFile << left << setw(5) << m.id << setw(18) << m.name << setw(16) << m.status
                 << setw(9) << (int)usage << "%" << setw(10) << (m.isShutDown ? "Yes" : "No") << endl;
    }
    dashFile << "\nSmart Tip: Shut down warning machines to prevent breakdown.\n";
    cout << CYAN << "\n--------------------------------------------------------\n" << RESET;
    dashFile.close();
}

// Prompts user to submit a kaizen improvement idea
void submitKaizen() {
    string idea;
    cin.ignore();
    cout << "Enter your improvement idea: ";
    getline(cin, idea);
    cout << GREEN << "Thank you! Your idea has been submitted.\n" << RESET;
}

// Saves machine histories to machine_history.txt before exiting
void writeHistoryToFile() {
    ofstream file("machine_history.txt");
    for (const auto& m : machines) {
        file << "Machine ID: " << m.id << " - " << m.name << "\n";
        for (const auto& h : m.history)
            file << "  " << h << "\n";
        file << "\n";
    }
    file.close();
    cout << CYAN << "Machine history saved to machine_history.txt\n" << RESET;
}

// Displays the main menu options for user interaction
void showMenu() {
    cout << CYAN << "\n==== KAIZEN MACHINERY MONITOR (REAL-TIME ONLY) ====\n" << RESET;
    cout << "1. Register Machine\n2. Predictive Maintenance Check\n3. View Machine List\n4. Mark Machine as Repaired\n5. Shut Down Warning Machine\n6. Show Dashboard\n7. Submit Kaizen Idea\n8. Exit\nChoice: ";
}

// Main program loop to handle user input and trigger features
int main() {
    int choice;
    while (true) {
        showMenu();  
        cin >> choice;
        switch (choice) {
            case 1: registerMachine(); break;
            case 2: predictiveMaintenanceCheck(); break;
            case 3: showMachines(); break;
            case 4: markRepaired(); break;
            case 5: shutDownWarningMachine(); break;
            case 6: showDashboard(); break;
            case 7: submitKaizen(); break;
            case 8: writeHistoryToFile(); return 0;
            default: cout << RED << "Invalid choice." << RESET << endl;
        }
    }
}