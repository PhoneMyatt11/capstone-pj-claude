#include "crow_all.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <cmath>
#include <memory>
#include <unordered_map>

// Safe conversion helpers
float safe_stof(const std::string &s, float def = 0.0f) {
    try { return std::stof(s); } catch (...) { return def; }
}

int safe_stoi(const std::string &s, int def = 0) {
    try { return std::stoi(s); } catch (...) { return def; }
}

// Decode application/x-www-form-urlencoded key/value
std::string urlDecode(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '+') {
            out.push_back(' ');
        } else if (s[i] == '%' && i + 2 < s.size()) {
            auto hex = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
                if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
                return 0;
            };
            int hi = hex(s[i + 1]);
            int lo = hex(s[i + 2]);
            out.push_back(static_cast<char>((hi << 4) | lo));
            i += 2;
        } else {
            out.push_back(s[i]);
        }
    }
    return out;
}

// Parse x-www-form-urlencoded body into a map
std::unordered_map<std::string, std::string> parseFormBody(const std::string& body) {
    std::unordered_map<std::string, std::string> result;
    size_t pos = 0;
    while (pos < body.size()) {
        size_t amp = body.find('&', pos);
        std::string pair = body.substr(pos, (amp == std::string::npos) ? std::string::npos : amp - pos);
        if (!pair.empty()) {
            size_t eq = pair.find('=');
            std::string key = urlDecode(pair.substr(0, eq));
            std::string val = (eq == std::string::npos) ? "" : urlDecode(pair.substr(eq + 1));
            result[key] = val;
        }
        if (amp == std::string::npos) break;
        pos = amp + 1;
    }
    return result;
}

// Entity structures
struct Airport {
    int id;
    std::string name;
    std::string city;
    std::string country;
    std::string iata;
    std::string icao;
    double latitude;
    double longitude;
    int altitude;
    float timezone;
    std::string dst;
    std::string tz_database;
    std::string type;
    std::string source;
};

struct Airline {
    int id;
    std::string name;
    std::string alias;
    std::string iata;
    std::string icao;
    std::string callsign;
    std::string country;
    std::string active;
};

struct Route {
    std::string airline_code;
    int airline_id;
    std::string source_airport;
    int source_airport_id;
    std::string dest_airport;
    int dest_airport_id;
    std::string codeshare;
    int stops;
    std::string equipment;
};

// Global data containers
std::unordered_map<std::string, std::shared_ptr<Airport>> airports_by_iata;
std::unordered_map<int, std::shared_ptr<Airport>> airports_by_id;
std::unordered_map<std::string, std::shared_ptr<Airline>> airlines_by_iata;
std::unordered_map<int, std::shared_ptr<Airline>> airlines_by_id;
std::vector<std::shared_ptr<Route>> routes;

// Session-based modifications (not persisted)
std::unordered_map<std::string, std::shared_ptr<Airport>> session_airports_by_iata;
std::unordered_map<int, std::shared_ptr<Airport>> session_airports_by_id;
std::unordered_map<std::string, std::shared_ptr<Airline>> session_airlines_by_iata;
std::unordered_map<int, std::shared_ptr<Airline>> session_airlines_by_id;
std::vector<std::shared_ptr<Route>> session_routes;

// Student information
const std::string STUDENT_ID = "20606537";
const std::string STUDENT_NAME = "Phone Myat Kyaw";

// Helper function to trim strings
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n\"");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n\"");
    return str.substr(first, (last - first + 1));
}

// Parse CSV line handling quoted fields
std::vector<std::string> parseCSVLine(const std::string& line) {
    std::vector<std::string> result;
    std::string field;
    bool in_quotes = false;
    
    for (size_t i = 0; i < line.length(); i++) {
        char c = line[i];
        if (c == '"') {
            in_quotes = !in_quotes;
        } else if (c == ',' && !in_quotes) {
            result.push_back(trim(field));
            field.clear();
        } else {
            field += c;
        }
    }
    result.push_back(trim(field));
    return result;
}

// Calculate distance between two coordinates (Haversine formula)
double calculateDistance(double lat1, double lon1, double lat2, double lon2) {
    const double R = 3958.8; // Earth radius in miles
    double dLat = (lat2 - lat1) * M_PI / 180.0;
    double dLon = (lon2 - lon1) * M_PI / 180.0;
    
    lat1 = lat1 * M_PI / 180.0;
    lat2 = lat2 * M_PI / 180.0;
    
    double a = sin(dLat/2) * sin(dLat/2) +
               cos(lat1) * cos(lat2) *
               sin(dLon/2) * sin(dLon/2);
    double c = 2 * atan2(sqrt(a), sqrt(1-a));
    return R * c;
}

// Load data from CSV files
void loadAirports(const std::string& filename) {
    std::ifstream file(filename);
    std::string line;
    
    while (std::getline(file, line)) {
        auto fields = parseCSVLine(line);
        if (fields.size() >= 14) {
            auto airport = std::make_shared<Airport>();
            airport->id        = safe_stoi(fields[0]);
            airport->name      = fields[1];
            airport->city      = fields[2];
            airport->country   = fields[3];
            airport->iata      = fields[4];
            airport->icao      = fields[5];
            airport->latitude  = safe_stof(fields[6], 0.0f);
            airport->longitude = safe_stof(fields[7], 0.0f);
            airport->altitude  = safe_stoi(fields[8]);
            airport->timezone  = safe_stof(fields[9], 0.0f);
            airport->dst       = fields[10];
            airport->tz_database = fields[11];
            airport->type      = fields[12];
            airport->source    = fields[13];
            
            if (!airport->iata.empty() && airport->iata != "\\N") {
                airports_by_iata[airport->iata] = airport;
            }
            airports_by_id[airport->id] = airport;
        }
    }
}

void loadAirlines(const std::string& filename) {
    std::ifstream file(filename);
    std::string line;
    
    while (std::getline(file, line)) {
        auto fields = parseCSVLine(line);
        if (fields.size() >= 8) {
            auto airline = std::make_shared<Airline>();
            airline->id      = safe_stoi(fields[0]);
            airline->name    = fields[1];
            airline->alias   = fields[2];
            airline->iata    = fields[3];
            airline->icao    = fields[4];
            airline->callsign= fields[5];
            airline->country = fields[6];
            airline->active  = fields[7];
            
            if (!airline->iata.empty() && airline->iata != "\\N") {
                airlines_by_iata[airline->iata] = airline;
            }
            airlines_by_id[airline->id] = airline;
        }
    }
}

void loadRoutes(const std::string& filename) {
    std::ifstream file(filename);
    std::string line;
    
    while (std::getline(file, line)) {
        auto fields = parseCSVLine(line);
        if (fields.size() >= 9) {
            auto route = std::make_shared<Route>();
            route->airline_code      = fields[0];
            route->airline_id        = safe_stoi(fields[1]);
            route->source_airport    = fields[2];
            route->source_airport_id = safe_stoi(fields[3]);
            route->dest_airport      = fields[4];
            route->dest_airport_id   = safe_stoi(fields[5]);
            route->codeshare         = fields[6];
            route->stops             = safe_stoi(fields[7]);
            route->equipment         = fields[8];
            
            routes.push_back(route);
        }
    }
}

// Initialize session data copies
void initializeSession() {
    session_airports_by_iata = airports_by_iata;
    session_airports_by_id = airports_by_id;
    session_airlines_by_iata = airlines_by_iata;
    session_airlines_by_id = airlines_by_id;
    session_routes = routes;
}

// HTML helper functions
std::string htmlHeader() {
    return R"(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>OpenFlights Air Travel Database</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 20px;
        }
        .container {
            max-width: 1200px;
            margin: 0 auto;
            background: white;
            border-radius: 15px;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
            overflow: hidden;
        }
        .header {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 30px;
            text-align: center;
        }
        .header h1 {
            font-size: 2.5em;
            margin-bottom: 10px;
            text-shadow: 2px 2px 4px rgba(0,0,0,0.2);
        }
        .header p {
            font-size: 1.1em;
            opacity: 0.9;
        }
        .nav {
            background: #f8f9fa;
            padding: 20px;
            border-bottom: 2px solid #e9ecef;
        }
        .nav-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 10px;
        }
        .nav-btn {
            background: white;
            border: 2px solid #667eea;
            color: #667eea;
            padding: 12px 20px;
            text-decoration: none;
            border-radius: 8px;
            text-align: center;
            font-weight: 600;
            transition: all 0.3s;
            display: block;
        }
        .nav-btn:hover {
            background: #667eea;
            color: white;
            transform: translateY(-2px);
            box-shadow: 0 4px 12px rgba(102, 126, 234, 0.4);
        }
        .content {
            padding: 30px;
        }
        .search-form {
            background: #f8f9fa;
            padding: 25px;
            border-radius: 10px;
            margin-bottom: 25px;
        }
        .form-group {
            margin-bottom: 20px;
        }
        .form-group label {
            display: block;
            margin-bottom: 8px;
            font-weight: 600;
            color: #333;
        }
        .form-group input, .form-group select {
            width: 100%;
            padding: 12px;
            border: 2px solid #e9ecef;
            border-radius: 8px;
            font-size: 1em;
            transition: border-color 0.3s;
        }
        .form-group input:focus, .form-group select:focus {
            outline: none;
            border-color: #667eea;
        }
        .btn {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 12px 30px;
            border: none;
            border-radius: 8px;
            font-size: 1em;
            font-weight: 600;
            cursor: pointer;
            transition: transform 0.3s, box-shadow 0.3s;
        }
        .btn:hover {
            transform: translateY(-2px);
            box-shadow: 0 6px 20px rgba(102, 126, 234, 0.4);
        }
        .result-box {
            background: #f8f9fa;
            padding: 20px;
            border-radius: 10px;
            border-left: 4px solid #667eea;
            margin-bottom: 20px;
        }
        .result-box h3 {
            color: #667eea;
            margin-bottom: 15px;
        }
        .result-item {
            background: white;
            padding: 15px;
            margin-bottom: 10px;
            border-radius: 8px;
            border: 1px solid #e9ecef;
        }
        .result-item strong {
            color: #667eea;
        }
        table {
            width: 100%;
            border-collapse: collapse;
            margin-top: 20px;
        }
        th, td {
            padding: 12px;
            text-align: left;
            border-bottom: 1px solid #e9ecef;
        }
        th {
            background: #667eea;
            color: white;
            font-weight: 600;
        }
        tr:hover {
            background: #f8f9fa;
        }
        .footer {
            background: #f8f9fa;
            padding: 20px;
            text-align: center;
            border-top: 2px solid #e9ecef;
            color: #666;
        }
        .feature-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
            gap: 20px;
            margin-top: 30px;
        }
        .feature-card {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 25px;
            border-radius: 10px;
            text-align: center;
            transition: transform 0.3s;
        }
        .feature-card:hover {
            transform: translateY(-5px);
        }
        .feature-card h3 {
            margin-bottom: 10px;
            font-size: 1.3em;
        }
        .code-display {
            background: #2d2d2d;
            color: #f8f8f2;
            padding: 20px;
            border-radius: 8px;
            overflow-x: auto;
            font-family: 'Courier New', monospace;
            font-size: 0.9em;
            line-height: 1.5;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>✈️ OpenFlights Air Travel Database</h1>
            <p>Comprehensive airline and airport data at your fingertips</p>
        </div>
        <div class="nav">
            <div class="nav-grid">
                <a href="/" class="nav-btn">🏠 Home</a>
                <a href="/airline" class="nav-btn">✈️ Search Airline</a>
                <a href="/airport" class="nav-btn">🛫 Search Airport</a>
                <a href="/reports" class="nav-btn">📊 Reports</a>
                <a href="/onehop" class="nav-btn">🔄 One-Hop Routes</a>
                <a href="/manage" class="nav-btn">⚙️ Manage Data</a>
                <a href="/code" class="nav-btn">💻 View Code</a>
                <a href="/about" class="nav-btn">ℹ️ About</a>
            </div>
        </div>
        <div class="content">
)";
}

std::string htmlFooter() {
    return R"(
        </div>
        <div class="footer">
            <p>Created by )" + STUDENT_NAME + R"( (ID: )" + STUDENT_ID + R"()</p>
            <p>CIS 22CH Honors Capstone Project | Powered by Crow C++ Framework</p>
        </div>
    </div>
</body>
</html>
)";
}

std::string htmlMessagePage(const std::string& title,
                            const std::string& message,
                            const std::string& color = "#28a745") // green default
{
    std::string html = htmlHeader();

    html += "<div class='result-box' style='border-left-color:" + color + ";'>";
    html += "<h3>" + title + "</h3>";
    html += "<p>" + message + "</p>";
    html += "</div>";

    html += "<p><a href='/manage' class='btn'>← Back to Manage Page</a></p>";

    html += htmlFooter();
    return html;
}

inline std::string successPage(const std::string& msg)
{
    return htmlMessagePage("Success", msg, "#28a745");  // green
}

inline std::string errorPage(const std::string& msg)
{
    return htmlMessagePage("Error", msg, "#dc3545");   // red
}

int main() {
    crow::SimpleApp app;

    // Load data
    loadAirports("airports.dat");
    loadAirlines("airlines.dat");
    loadRoutes("routes.dat");
    initializeSession();

    // Home page
    CROW_ROUTE(app, "/")([](){
        std::string html = htmlHeader();
        html += R"(
            <h2>Welcome to OpenFlights Database</h2>
            <p style="margin: 20px 0; font-size: 1.1em; line-height: 1.6;">
                This web application provides comprehensive access to airline, airport, and route data 
                from the OpenFlights database. Explore flight connections, search for specific airlines 
                and airports, generate detailed reports, and discover optimal flight paths.
            </p>
            
            <div class="feature-grid">
                <div class="feature-card">
                    <h3>🔍 Search</h3>
                    <p>Find airlines and airports by IATA code</p>
                </div>
                <div class="feature-card">
                    <h3>📊 Reports</h3>
                    <p>Generate comprehensive data reports</p>
                </div>
                <div class="feature-card">
                    <h3>🔄 One-Hop</h3>
                    <p>Find connecting flights between airports</p>
                </div>
                <div class="feature-card">
                    <h3>⚙️ Manage</h3>
                    <p>Add, update, and delete data entries</p>
                </div>
            </div>
            
            <div class="result-box" style="margin-top: 30px;">
                <h3>📈 Database Statistics</h3>
                <div class="result-item">
                    <strong>Total Airlines:</strong> )" + std::to_string(airlines_by_id.size()) + R"(
                </div>
                <div class="result-item">
                    <strong>Total Airports:</strong> )" + std::to_string(airports_by_id.size()) + R"(
                </div>
                <div class="result-item">
                    <strong>Total Routes:</strong> )" + std::to_string(routes.size()) + R"(
                </div>
            </div>
        )";
        html += htmlFooter();
        return html;
    });

    // Get student ID
    CROW_ROUTE(app, "/id")([](){
        crow::json::wvalue result;
        result["student_id"] = STUDENT_ID;
        result["name"] = STUDENT_NAME;
        return result;
    });

    // Search airline by IATA
    CROW_ROUTE(app, "/airline")([](const crow::request& req){
        std::string html = htmlHeader();
        html += R"(
            <h2>Search Airline by IATA Code</h2>
            <div class="search-form">
                <form method="GET" action="/airline/search">
                    <div class="form-group">
                        <label for="iata">Enter Airline IATA Code (e.g., AA, UA, DL):</label>
                        <input type="text" id="iata" name="iata" placeholder="AA" maxlength="3" required>
                    </div>
                    <button type="submit" class="btn">Search Airline</button>
                </form>
            </div>
        )";
        html += htmlFooter();
        return html;
    });

    CROW_ROUTE(app, "/airline/search")([](const crow::request& req){
        auto iata = req.url_params.get("iata");
        std::string html = htmlHeader();
        
        if (iata) {
            std::string iata_code = iata;
            std::transform(iata_code.begin(), iata_code.end(), iata_code.begin(), ::toupper);
            
            auto it = session_airlines_by_iata.find(iata_code);
            if (it != session_airlines_by_iata.end()) {
                auto airline = it->second;
                html += R"(<h2>Airline Details</h2>)";
                html += R"(<div class="result-box">)";
                html += "<div class='result-item'><strong>ID:</strong> " + std::to_string(airline->id) + "</div>";
                html += "<div class='result-item'><strong>Name:</strong> " + airline->name + "</div>";
                html += "<div class='result-item'><strong>Alias:</strong> " + airline->alias + "</div>";
                html += "<div class='result-item'><strong>IATA:</strong> " + airline->iata + "</div>";
                html += "<div class='result-item'><strong>ICAO:</strong> " + airline->icao + "</div>";
                html += "<div class='result-item'><strong>Callsign:</strong> " + airline->callsign + "</div>";
                html += "<div class='result-item'><strong>Country:</strong> " + airline->country + "</div>";
                html += "<div class='result-item'><strong>Active:</strong> " + airline->active + "</div>";
                html += "</div>";
            } else {
                html += R"(<div class="result-box" style="border-left-color: #dc3545;">)";
                html += "<p>❌ Airline with IATA code '" + iata_code + "' not found.</p>";
                html += "</div>";
            }
        }
        
        html += "<p><a href='/airline' class='btn'>🔙 Search Another Airline</a></p>";
        html += htmlFooter();
        return html;
    });

    // Search airport by IATA
    CROW_ROUTE(app, "/airport")([](const crow::request& req){
        std::string html = htmlHeader();
        html += R"(
            <h2>Search Airport by IATA Code</h2>
            <div class="search-form">
                <form method="GET" action="/airport/search">
                    <div class="form-group">
                        <label for="iata">Enter Airport IATA Code (e.g., SFO, ORD, JFK):</label>
                        <input type="text" id="iata" name="iata" placeholder="SFO" maxlength="3" required>
                    </div>
                    <button type="submit" class="btn">Search Airport</button>
                </form>
            </div>
        )";
        html += htmlFooter();
        return html;
    });

    CROW_ROUTE(app, "/airport/search")([](const crow::request& req){
        auto iata = req.url_params.get("iata");
        std::string html = htmlHeader();
        
        if (iata) {
            std::string iata_code = iata;
            std::transform(iata_code.begin(), iata_code.end(), iata_code.begin(), ::toupper);
            
            auto it = session_airports_by_iata.find(iata_code);
            if (it != session_airports_by_iata.end()) {
                auto airport = it->second;
                html += R"(<h2>Airport Details</h2>)";
                html += R"(<div class="result-box">)";
                html += "<div class='result-item'><strong>ID:</strong> " + std::to_string(airport->id) + "</div>";
                html += "<div class='result-item'><strong>Name:</strong> " + airport->name + "</div>";
                html += "<div class='result-item'><strong>City:</strong> " + airport->city + "</div>";
                html += "<div class='result-item'><strong>Country:</strong> " + airport->country + "</div>";
                html += "<div class='result-item'><strong>IATA:</strong> " + airport->iata + "</div>";
                html += "<div class='result-item'><strong>ICAO:</strong> " + airport->icao + "</div>";
                html += "<div class='result-item'><strong>Latitude:</strong> " + std::to_string(airport->latitude) + "</div>";
                html += "<div class='result-item'><strong>Longitude:</strong> " + std::to_string(airport->longitude) + "</div>";
                html += "<div class='result-item'><strong>Altitude:</strong> " + std::to_string(airport->altitude) + " ft</div>";
                html += "<div class='result-item'><strong>Timezone:</strong> " + std::to_string(airport->timezone) + "</div>";
                html += "<div class='result-item'><strong>DST:</strong> " + airport->dst + "</div>";
                html += "<div class='result-item'><strong>TZ Database:</strong> " + airport->tz_database + "</div>";
                html += "<div class='result-item'><strong>Type:</strong> " + airport->type + "</div>";
                html += "<div class='result-item'><strong>Source:</strong> " + airport->source + "</div>";
                html += "</div>";
            } else {
                html += R"(<div class="result-box" style="border-left-color: #dc3545;">)";
                html += "<p>❌ Airport with IATA code '" + iata_code + "' not found.</p>";
                html += "</div>";
            }
        }
        
        html += "<p><a href='/airport' class='btn'>🔙 Search Another Airport</a></p>";
        html += htmlFooter();
        return html;
    });

    // Reports page
    CROW_ROUTE(app, "/reports")([](const crow::request& req){
        std::string html = htmlHeader();
        html += R"(
            <h2>📊 Generate Reports</h2>
            
            <div class="search-form">
                <h3>Airlines Ordered by IATA Code</h3>
                <form method="GET" action="/reports/airlines">
                    <button type="submit" class="btn">Generate Airlines Report</button>
                </form>
            </div>
            
            <div class="search-form">
                <h3>Airports Ordered by IATA Code</h3>
                <form method="GET" action="/reports/airports">
                    <button type="submit" class="btn">Generate Airports Report</button>
                </form>
            </div>
            
            <div class="search-form">
                <h3>Airports by Airline Routes</h3>
                <form method="GET" action="/reports/airline-routes">
                    <div class="form-group">
                        <label for="iata">Enter Airline IATA Code:</label>
                        <input type="text" id="iata" name="iata" placeholder="AA" maxlength="3" required>
                    </div>
                    <button type="submit" class="btn">Generate Report</button>
                </form>
            </div>
            
            <div class="search-form">
                <h3>Airlines by Airport Routes</h3>
                <form method="GET" action="/reports/airport-routes">
                    <div class="form-group">
                        <label for="iata">Enter Airport IATA Code:</label>
                        <input type="text" id="iata" name="iata" placeholder="SFO" maxlength="3" required>
                    </div>
                    <button type="submit" class="btn">Generate Report</button>
                </form>
            </div>
        )";
        html += htmlFooter();
        return html;
    });

    // About page with Get ID
    CROW_ROUTE(app, "/about")([](const crow::request& req){
        std::string html = htmlHeader();
        html += R"(
            <h2>ℹ️ About This Project</h2>
            <div class="result-box">
                <h3>Student Information</h3>
                <div class="result-item">
                    <strong>Name:</strong> )" + STUDENT_NAME + R"(
                </div>
                <div class="result-item">
                    <strong>De Anza Student ID:</strong> )" + STUDENT_ID + R"(
                </div>
            </div>
            
            <div class="result-box">
                <h3>Project Details</h3>
                <p style="line-height: 1.8;">
                    This web application was created as part of the CIS 22CH Honors Capstone Project 
                    using <strong>Vibe Coding</strong> techniques. The project demonstrates the integration 
                    of AI-assisted development with traditional C++ programming to create a functional 
                    web service.
                </p>
                <p style="margin-top: 15px; line-height: 1.8;">
                    <strong>Technologies Used:</strong>
                </p>
                <ul style="margin-left: 20px; line-height: 1.8;">
                    <li>C++ (with modern features)</li>
                    <li>Crow C++ Web Framework</li>
                    <li>STL containers (unordered_map, vector, set)</li>
                    <li>Smart pointers for memory management</li>
                    <li>HTML/CSS for frontend</li>
                </ul>
            </div>
            
            <div class="result-box">
                <h3>Features Implemented</h3>
                <ul style="margin-left: 20px; line-height: 1.8;">
                    <li>✅ Entity retrieval by IATA code</li>
                    <li>✅ Comprehensive reporting system</li>
                    <li>✅ Route analysis and calculations</li>
                    <li>✅ One-hop route finding</li>
                    <li>✅ Data management (CRUD operations)</li>
                    <li>✅ Enhanced UI with modern design</li>
                    <li>✅ Source code viewing</li>
                </ul>
            </div>
        )";
        html += htmlFooter();
        return html;
    });

    // View source code
    CROW_ROUTE(app, "/code")([](const crow::request& req){
        std::string html = htmlHeader();
        html += R"(
            <h2>💻 Source Code</h2>
            <p>Below is the complete C++ source code for this web application, generated through Vibe Coding:</p>
            <div class="code-display">
        )";
        
        // Display a simplified version of the code
        html += R"CODE(
#include "crow_all.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <cmath>
#include <memory>
#include <unordered_map>

// [Full implementation of Airport, Airline, and Route structures]
// [Data loading functions for CSV parsing]
// [Route handlers for all endpoints]
// [HTML generation functions]
// [Distance calculation using Haversine formula]
// [Session-based data management]

// This code demonstrates:
// - Modern C++ with smart pointers
// - STL containers (unordered_map, vector, set)
// - Crow web framework for HTTP handling
// - CSV data parsing
// - Geographical calculations
// - RESTful API design
// - HTML templating in C++
)CODE";
        html += R"(
            </div>
            <p style="margin-top: 20px;">
                <a href="/code/download" class="btn">⬇️ Download Full Source Code</a>
                <a href="https://github.com/PhoneMyatt11/capstone-pj-claude.git" class="btn" target="_blank" style="margin-left:10px;">
                    🌐 View on GitHub
                </a>
            </p>
        )";
        html += htmlFooter();
        return html;
    });

    // Downloadable source code
    CROW_ROUTE(app, "/code/download")
([](){
    std::ifstream file("openflights_web_service.cpp");
    if (!file.is_open()) {
        return crow::response(500, "Error: Could not open source file.");
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    crow::response resp(buffer.str());
    resp.code = 200;
    resp.add_header("Content-Type", "text/plain");
    resp.add_header("Content-Disposition", "attachment; filename=\"openflights_web_service.cpp\"");
    return resp;
});

    // One-hop routes
    CROW_ROUTE(app, "/onehop")([](const crow::request& req){
        std::string html = htmlHeader();
        html += R"(
            <h2>🔄 One-Hop Route Finder</h2>
            <p>Find all one-stop routes between two airports (routes with exactly one connection).</p>
            <div class="search-form">
                <form method="GET" action="/onehop/search">
                    <div class="form-group">
                        <label for="source">Source Airport IATA Code:</label>
                        <input type="text" id="source" name="source" placeholder="SFO" maxlength="3" required>
                    </div>
                    <div class="form-group">
                        <label for="dest">Destination Airport IATA Code:</label>
                        <input type="text" id="dest" name="dest" placeholder="ORD" maxlength="3" required>
                    </div>
                    <button type="submit" class="btn">Find Routes</button>
                </form>
            </div>
        )";
        html += htmlFooter();
        return html;
    });

    CROW_ROUTE(app, "/onehop/search")([](const crow::request& req){
        auto source_param = req.url_params.get("source");
        auto dest_param = req.url_params.get("dest");
        
        std::string html = htmlHeader();
        html += R"(<h2>🔄 One-Hop Route Results</h2>)";
        
        if (source_param && dest_param) {
            std::string source = source_param;
            std::string dest = dest_param;
            std::transform(source.begin(), source.end(), source.begin(), ::toupper);
            std::transform(dest.begin(), dest.end(), dest.begin(), ::toupper);
            
            auto source_it = session_airports_by_iata.find(source);
            auto dest_it = session_airports_by_iata.find(dest);
            
            if (source_it != session_airports_by_iata.end() && dest_it != session_airports_by_iata.end()) {
                auto source_airport = source_it->second;
                auto dest_airport = dest_it->second;
                
                // Find one-hop routes
                struct RouteInfo {
                    std::string intermediate;
                    std::string airline1;
                    std::string airline2;
                    double distance;
                };
                
                std::vector<RouteInfo> one_hop_routes;
                
                // Find routes from source
                std::set<std::string> intermediates;
                for (const auto& route : session_routes) {
                    if (route->source_airport == source && route->stops == 0) {
                        intermediates.insert(route->dest_airport);
                    }
                }
                
                // Find routes from intermediates to destination
                for (const auto& intermediate : intermediates) {
                    for (const auto& route : session_routes) {
                        if (route->source_airport == intermediate && 
                            route->dest_airport == dest && 
                            route->stops == 0) {
                            
                            // Find airline names
                            std::string airline1 = "Unknown";
                            std::string airline2 = "Unknown";
                            
                            for (const auto& r1 : session_routes) {
                                if (r1->source_airport == source && r1->dest_airport == intermediate) {
                                    auto a_it = session_airlines_by_iata.find(r1->airline_code);
                                    if (a_it != session_airlines_by_iata.end()) {
                                        airline1 = a_it->second->name;
                                    }
                                    break;
                                }
                            }
                            
                            auto a_it = session_airlines_by_iata.find(route->airline_code);
                            if (a_it != session_airlines_by_iata.end()) {
                                airline2 = a_it->second->name;
                            }
                            
                            // Calculate distance
                            auto inter_it = session_airports_by_iata.find(intermediate);
                            if (inter_it != session_airports_by_iata.end()) {
                                auto inter_airport = inter_it->second;
                                double dist1 = calculateDistance(
                                    source_airport->latitude, source_airport->longitude,
                                    inter_airport->latitude, inter_airport->longitude
                                );
                                double dist2 = calculateDistance(
                                    inter_airport->latitude, inter_airport->longitude,
                                    dest_airport->latitude, dest_airport->longitude
                                );
                                
                                RouteInfo info;
                                info.intermediate = intermediate;
                                info.airline1 = airline1;
                                info.airline2 = airline2;
                                info.distance = dist1 + dist2;
                                one_hop_routes.push_back(info);
                            }
                        }
                    }
                }
                
                // Sort by distance
                std::sort(one_hop_routes.begin(), one_hop_routes.end(),
                    [](const RouteInfo& a, const RouteInfo& b) {
                        return a.distance < b.distance;
                    });
                
                if (!one_hop_routes.empty()) {
                    html += "<div class='result-box'>";
                    html += "<h3>Found " + std::to_string(one_hop_routes.size()) + " one-hop route(s)</h3>";
                    html += "<table><thead><tr>";
                    html += "<th>Rank</th><th>Route</th><th>Airlines</th><th>Total Distance (miles)</th>";
                    html += "</tr></thead><tbody>";
                    
                    int rank = 1;
                    for (const auto& route_info : one_hop_routes) {
                        html += "<tr>";
                        html += "<td>" + std::to_string(rank++) + "</td>";
                        html += "<td>" + source + " → " + route_info.intermediate + " → " + dest + "</td>";
                        html += "<td>" + route_info.airline1 + " / " + route_info.airline2 + "</td>";
                        html += "<td>" + std::to_string(static_cast<int>(route_info.distance)) + "</td>";
                        html += "</tr>";
                    }
                    
                    html += "</tbody></table></div>";
                } else {
                    html += "<div class='result-box' style='border-left-color: #ffc107;'>";
                    html += "<p>⚠️ No one-hop routes found between " + source + " and " + dest + "</p>";
                    html += "</div>";
                }
            } else {
                html += "<div class='result-box' style='border-left-color: #dc3545;'>";
                html += "<p>❌ One or both airports not found.</p>";
                html += "</div>";
            }
        }
        
        html += "<p><a href='/onehop' class='btn'>🔙 Search Again</a></p>";
        html += htmlFooter();
        return html;
    });

    // Data management page
    CROW_ROUTE(app, "/manage")([](const crow::request& req){
        std::string html = htmlHeader();
        html += R"(
            <h2>⚙️ Data Management</h2>
            <p><strong>Note:</strong> All modifications are session-based and will reset when the server restarts.</p>
            
            <div class="search-form">
                <h3>Insert Airline</h3>
                <form method="POST" action="/manage/airline/insert">
                    <div class="form-group">
                        <label>ID:</label>
                        <input type="number" name="id" required>
                    </div>
                    <div class="form-group">
                        <label>IATA:</label>
                        <input type="text" name="iata" maxlength="3" required>
                    </div>
                    <div class="form-group">
                        <label>Name:</label>
                        <input type="text" name="name" required>
                    </div>
                    <div class="form-group">
                        <label>Country:</label>
                        <input type="text" name="country" required>
                    </div>
                    <button class="btn">Insert</button>
                </form>
            </div>

            <div class="search-form">
                <h3>Modify Airline</h3>
                <form method="POST" action="/manage/airline/modify">
                    <div class="form-group">
                        <label>IATA (unchanged):</label>
                        <input type="text" name="iata" maxlength="3" required>
                    </div>
                    <div class="form-group">
                        <label>Name (optional):</label>
                        <input type="text" name="name">
                    </div>
                    <div class="form-group">
                        <label>Country (optional):</label>
                        <input type="text" name="country">
                    </div>
                    <button class="btn">Modify</button>
                </form>
            </div>
            
            <div class="search-form">
                <h3>Delete Airline</h3>
                <form method="POST" action="/manage/airline/delete">
                    <div class="form-group">
                        <label>IATA:</label>
                        <input type="text" name="iata" maxlength="3" required>
                    </div>
                    <button class="btn" style="background:#d9534f;">Delete</button>
                </form>
            </div>

            <div class="search-form">
                <h3>Insert Airport</h3>
                <form method="POST" action="/manage/airport/insert">
                    <div class="form-group">
                        <label>ID:</label>
                        <input type="number" name="id" required>
                    </div>
                    <div class="form-group">
                        <label>IATA:</label>
                        <input type="text" name="iata" maxlength="3" required>
                    </div>
                    <div class="form-group">
                        <label>Name:</label>
                        <input type="text" name="name" required>
                    </div>
                    <div class="form-group">
                        <label>City:</label>
                        <input type="text" name="city" required>
                    </div>
                    <div class="form-group">
                        <label>Country:</label>
                        <input type="text" name="country" required>
                    </div>
                    <button class="btn">Insert</button>
                </form>
            </div>

            <div class="search-form">
                <h3>Modify Airport</h3>
                <form method="POST" action="/manage/airport/modify">
                    <div class="form-group">
                        <label>IATA (unchanged):</label>
                        <input type="text" name="iata" maxlength="3" required>
                    </div>
                    <div class="form-group">
                        <label>Name (optional):</label>
                        <input type="text" name="name">
                    </div>
                    <div class="form-group">
                        <label>City (optional):</label>
                        <input type="text" name="city">
                    </div>
                    <div class="form-group">
                        <label>Country (optional):</label>
                        <input type="text" name="country">
                    </div>
                    <button class="btn">Modify</button>
                </form>
            </div>

            <div class="search-form">
                <h3>Delete Airport</h3>
                <form method="POST" action="/manage/airport/delete">
                    <div class="form-group">
                        <label>IATA:</label>
                        <input type="text" name="iata" maxlength="3" required>
                    </div>
                    <button class="btn" style="background:#d9534f;">Delete</button>
                </form>
            </div>

            <div class="search-form">
                <h3>Insert Route</h3>
                <form method="POST" action="/manage/route/insert">
                    <div class="form-group">
                        <label>Airline IATA:</label>
                        <input type="text" name="airline" maxlength="3" required>
                    </div>
                    <div class="form-group">
                        <label>Source Airport IATA:</label>
                        <input type="text" name="source" maxlength="3" required>
                    </div>
                    <div class="form-group">
                        <label>Destination Airport IATA:</label>
                        <input type="text" name="dest" maxlength="3" required>
                    </div>
                    <button class="btn">Insert</button>
                </form>
            </div>

            <div class="search-form">
                <h3>Delete Route</h3>
                <form method="POST" action="/manage/route/delete">
                    <div class="form-group">
                        <label>Airline IATA:</label>
                        <input type="text" name="airline" maxlength="3" required>
                    </div>
                    <div class="form-group">
                        <label>Source Airport IATA:</label>
                        <input type="text" name="source" maxlength="3" required>
                    </div>
                    <div class="form-group">
                        <label>Destination Airport IATA:</label>
                        <input type="text" name="dest" maxlength="3" required>
                    </div>
                    <button class="btn" style="background:#d9534f;">Delete</button>
                </form>
            </div>
        )";
        html += htmlFooter();
        return html;
    });

    // Report handlers (continued)
    CROW_ROUTE(app, "/reports/airlines")([](const crow::request& req){
        std::string html = htmlHeader();
        html += R"(<h2>📊 All Airlines (Ordered by IATA Code)</h2>)";
        
        std::vector<std::shared_ptr<Airline>> sorted_airlines;
        for (const auto& pair : session_airlines_by_iata) {
            sorted_airlines.push_back(pair.second);
        }
        
        std::sort(sorted_airlines.begin(), sorted_airlines.end(),
            [](const std::shared_ptr<Airline>& a, const std::shared_ptr<Airline>& b) {
                return a->iata < b->iata;
            });
        
        html += "<div class='result-box'>";
        html += "<p>Total Airlines: " + std::to_string(sorted_airlines.size()) + "</p>";
        html += "<table><thead><tr>";
        html += "<th>IATA</th><th>Name</th><th>Country</th><th>Active</th>";
        html += "</tr></thead><tbody>";
        
        for (const auto& airline : sorted_airlines) {
            html += "<tr>";
            html += "<td>" + airline->iata + "</td>";
            html += "<td>" + airline->name + "</td>";
            html += "<td>" + airline->country + "</td>";
            html += "<td>" + airline->active + "</td>";
            html += "</tr>";
        }
        
        html += "</tbody></table></div>";
        html += "<p><a href='/reports' class='btn'>🔙 Back to Reports</a></p>";
        html += htmlFooter();
        return html;
    });

    CROW_ROUTE(app, "/reports/airports")([](const crow::request& req){
        std::string html = htmlHeader();
        html += R"(<h2>📊 All Airports (Ordered by IATA Code)</h2>)";
        
        std::vector<std::shared_ptr<Airport>> sorted_airports;
        for (const auto& pair : session_airports_by_iata) {
            sorted_airports.push_back(pair.second);
        }
        
        std::sort(sorted_airports.begin(), sorted_airports.end(),
            [](const std::shared_ptr<Airport>& a, const std::shared_ptr<Airport>& b) {
                return a->iata < b->iata;
            });
        
        html += "<div class='result-box'>";
        html += "<p>Total Airports: " + std::to_string(sorted_airports.size()) + "</p>";
        html += "<table><thead><tr>";
        html += "<th>IATA</th><th>Name</th><th>City</th><th>Country</th>";
        html += "</tr></thead><tbody>";
        
        for (const auto& airport : sorted_airports) {
            html += "<tr>";
            html += "<td>" + airport->iata + "</td>";
            html += "<td>" + airport->name + "</td>";
            html += "<td>" + airport->city + "</td>";
            html += "<td>" + airport->country + "</td>";
            html += "</tr>";
        }
        
        html += "</tbody></table></div>";
        html += "<p><a href='/reports' class='btn'>🔙 Back to Reports</a></p>";
        html += htmlFooter();
        return html;
    });

    CROW_ROUTE(app, "/reports/airline-routes")([](const crow::request& req) {
        auto iata = req.url_params.get("iata");
        std::string html = htmlHeader();

        html += R"(<h2>📊 Airline Route Report</h2>)";

        if (!iata) {
            html += R"(<div class="result-box" style="border-left-color:#dc3545;">
                        <p>❌ Missing IATA parameter.</p></div>)";
            html += htmlFooter();
            return html;
        }

        std::string airline_code = iata;
        std::transform(airline_code.begin(), airline_code.end(), airline_code.begin(), ::toupper);

        auto it = session_airlines_by_iata.find(airline_code);
        if (it == session_airlines_by_iata.end()) {
            html += R"(<div class="result-box" style="border-left-color:#dc3545;">
                        <p>❌ Airline not found.</p></div>)";
            html += htmlFooter();
            return html;
        }

        auto airline = it->second;

        // Count airport occurrences
        std::unordered_map<std::string, int> airport_counts;

        for (auto& route : session_routes) {
            if (route->stops == 0 && route->airline_code == airline_code) {
                if (!route->source_airport.empty())
                    airport_counts[route->source_airport]++;
                if (!route->dest_airport.empty())
                    airport_counts[route->dest_airport]++;
            }
        }

        // Convert to vector for sorting
        std::vector<std::pair<std::string, int>> sorted;
        for (auto& pair : airport_counts) sorted.push_back(pair);

        std::sort(sorted.begin(), sorted.end(),
                  [](auto& a, auto& b) { return a.second > b.second; });

        // Build HTML
        html += "<div class='result-box'>";
        html += "<h3>Airline: " + airline->name +
                " (" + airline_code + ")</h3>";
        html += "<p>Total connected airports: " + std::to_string(sorted.size()) + "</p>";

        html += R"(
        <table>
            <thead>
                <tr>
                    <th>Airport</th>
                    <th>City</th>
                    <th>Country</th>
                    <th>Routes</th>
                </tr>
            </thead>
            <tbody>
    )";

        for (auto& p : sorted) {
            auto airport_it = session_airports_by_iata.find(p.first);
            if (airport_it != session_airports_by_iata.end()) {
                auto ap = airport_it->second;

                html += "<tr>";
                html += "<td>" + ap->iata + " (" + ap->name + ")</td>";
                html += "<td>" + ap->city + "</td>";
                html += "<td>" + ap->country + "</td>";
                html += "<td>" + std::to_string(p.second) + "</td>";
                html += "</tr>";
            }
        }

        html += "</tbody></table></div>";
        html += "<p><a href='/reports' class='btn'>🔙 Back to Reports</a></p>";
        html += htmlFooter();
        return html;
    });

    CROW_ROUTE(app, "/reports/airport-routes")([](const crow::request& req) {
        auto iata = req.url_params.get("iata");
        std::string html = htmlHeader();

        html += R"(<h2>📊 Airport Route Report</h2>)";

        if (!iata) {
            html += R"(<div class="result-box" style="border-left-color:#dc3545;">
                        <p>❌ Missing IATA parameter.</p></div>)";
            html += htmlFooter();
            return html;
        }

        std::string airport_code = iata;
        std::transform(airport_code.begin(), airport_code.end(), airport_code.begin(), ::toupper);

        auto it = session_airports_by_iata.find(airport_code);
        if (it == session_airports_by_iata.end()) {
            html += R"(<div class="result-box" style="border-left-color:#dc3545;">
                        <p>❌ Airport not found.</p></div>)";
            html += htmlFooter();
            return html;
        }

        auto airport = it->second;

        // Count airlines serving this airport
        std::unordered_map<std::string, int> airline_counts;

        for (auto& route : session_routes) {
            if (route->stops == 0 &&
                (route->source_airport == airport_code || route->dest_airport == airport_code)) {

                if (!route->airline_code.empty())
                    airline_counts[route->airline_code]++;
            }
        }

        // Convert to vector for sorting
        std::vector<std::pair<std::string, int>> sorted;
        for (auto& pair : airline_counts) sorted.push_back(pair);

        std::sort(sorted.begin(), sorted.end(),
                  [](auto& a, auto& b) { return a.second > b.second; });

        // Build HTML
        html += "<div class='result-box'>";
        html += "<h3>Airport: " + airport->name +
                " (" + airport_code + ")</h3>";
        html += "<p>Total airlines serving this airport: " + std::to_string(sorted.size()) + "</p>";

        html += R"(
        <table>
            <thead>
                <tr>
                    <th>Airline</th>
                    <th>Country</th>
                    <th>Routes</th>
                </tr>
            </thead>
            <tbody>
    )";

        for (auto& p : sorted) {
            auto airline_it = session_airlines_by_iata.find(p.first);

            html += "<tr>";

            if (airline_it != session_airlines_by_iata.end()) {
                auto al = airline_it->second;
                html += "<td>" + al->name + " (" + al->iata + ")</td>";
                html += "<td>" + al->country + "</td>";
            } else {
                html += "<td>Unknown (" + p.first + ")</td><td>Unknown</td>";
            }

            html += "<td>" + std::to_string(p.second) + "</td>";
            html += "</tr>";
        }

        html += "</tbody></table></div>";
        html += "<p><a href='/reports' class='btn'>🔙 Back to Reports</a></p>";
        html += htmlFooter();
        return html;
    });

    // Airline – INSERT (HTML response)
    CROW_ROUTE(app, "/manage/airline/insert").methods("POST"_method)
    ([](const crow::request& req) {
        auto form = parseFormBody(req.body);

        auto id_it      = form.find("id");
        auto iata_it    = form.find("iata");
        auto name_it    = form.find("name");
        auto country_it = form.find("country");

        if (id_it == form.end() || iata_it == form.end() ||
            name_it == form.end() || country_it == form.end() ||
            id_it->second.empty() || iata_it->second.empty() ||
            name_it->second.empty() || country_it->second.empty())
        {
            return crow::response(errorPage("Missing required parameters."));
        }

        int id = safe_stoi(id_it->second);
        std::string iata    = iata_it->second;
        std::string name    = name_it->second;
        std::string country = country_it->second;

        std::transform(iata.begin(), iata.end(), iata.begin(), ::toupper);

        if (session_airlines_by_id.count(id))
            return crow::response(errorPage("Airline ID already exists."));

        if (session_airlines_by_iata.count(iata))
            return crow::response(errorPage("Airline IATA already exists."));

        auto al = std::make_shared<Airline>();
        al->id      = id;
        al->iata    = iata;
        al->name    = name;
        al->country = country;

        session_airlines_by_id[id]     = al;
        session_airlines_by_iata[iata] = al;

        return crow::response(successPage("Airline inserted successfully!"));
    });

    // Airline – MODIFY (HTML response)
    CROW_ROUTE(app, "/manage/airline/modify").methods("POST"_method)
    ([](const crow::request& req) {
        auto form = parseFormBody(req.body);

        auto iata_it = form.find("iata");
        if (iata_it == form.end() || iata_it->second.empty())
            return crow::response(errorPage("Missing IATA parameter."));

        std::string iata = iata_it->second;
        std::transform(iata.begin(), iata.end(), iata.begin(), ::toupper);

        auto it = session_airlines_by_iata.find(iata);
        if (it == session_airlines_by_iata.end())
            return crow::response(errorPage("Airline not found."));

        auto al = it->second;

        auto name_it    = form.find("name");
        auto country_it = form.find("country");

        if (name_it != form.end() && !name_it->second.empty())
            al->name = name_it->second;

        if (country_it != form.end() && !country_it->second.empty())
            al->country = country_it->second;

        return crow::response(successPage("Airline modified successfully!"));
    });

    // Airline – DELETE (HTML response)
    CROW_ROUTE(app, "/manage/airline/delete").methods("POST"_method)
    ([](const crow::request& req) {
        auto form = parseFormBody(req.body);
        auto iata_it = form.find("iata");

        if (iata_it == form.end() || iata_it->second.empty())
            return crow::response(errorPage("Missing IATA parameter."));

        std::string iata = iata_it->second;
        std::transform(iata.begin(), iata.end(), iata.begin(), ::toupper);

        auto it = session_airlines_by_iata.find(iata);
        if (it == session_airlines_by_iata.end())
            return crow::response(errorPage("Airline not found."));

        int id = it->second->id;

        session_airlines_by_iata.erase(iata);
        session_airlines_by_id.erase(id);

        session_routes.erase(
            std::remove_if(
                session_routes.begin(),
                session_routes.end(),
                [&](const std::shared_ptr<Route>& r) {
                    return r->airline_code == iata;
                }
            ),
            session_routes.end()
        );

        return crow::response(successPage("Airline and all related routes deleted."));
    });

    // Airport – INSERT (HTML response)
    CROW_ROUTE(app, "/manage/airport/insert").methods("POST"_method)
    ([](const crow::request& req) {
        auto form = parseFormBody(req.body);

        auto id_it      = form.find("id");
        auto iata_it    = form.find("iata");
        auto name_it    = form.find("name");
        auto city_it    = form.find("city");
        auto country_it = form.find("country");

        if (id_it == form.end() || iata_it == form.end() ||
            name_it == form.end() || city_it == form.end() ||
            country_it == form.end() ||
            id_it->second.empty() || iata_it->second.empty() ||
            name_it->second.empty() || city_it->second.empty() ||
            country_it->second.empty()) {
            return crow::response(errorPage("Missing required parameters."));
        }

        int id = safe_stoi(id_it->second);
        std::string iata    = iata_it->second;
        std::string name    = name_it->second;
        std::string city    = city_it->second;
        std::string country = country_it->second;
        std::transform(iata.begin(), iata.end(), iata.begin(), ::toupper);

        if (session_airports_by_id.count(id))
            return crow::response(errorPage("Airport ID already exists."));

        if (session_airports_by_iata.count(iata))
            return crow::response(errorPage("Airport IATA already exists."));

        auto ap = std::make_shared<Airport>();
        ap->id      = id;
        ap->iata    = iata;
        ap->name    = name;
        ap->city    = city;
        ap->country = country;

        session_airports_by_id[id]     = ap;
        session_airports_by_iata[iata] = ap;

        return crow::response(successPage("Airport inserted successfully!"));
    });

    // Airport – MODIFY (HTML response)
    CROW_ROUTE(app, "/manage/airport/modify").methods("POST"_method)
    ([](const crow::request& req) {
        auto form = parseFormBody(req.body);

        auto iata_it = form.find("iata");
        if (iata_it == form.end() || iata_it->second.empty())
            return crow::response(errorPage("Missing IATA parameter."));

        std::string iata = iata_it->second;
        std::transform(iata.begin(), iata.end(), iata.begin(), ::toupper);

        auto it = session_airports_by_iata.find(iata);
        if (it == session_airports_by_iata.end())
            return crow::response(errorPage("Airport not found."));

        auto ap = it->second;

        auto name_it    = form.find("name");
        auto city_it    = form.find("city");
        auto country_it = form.find("country");

        if (name_it != form.end() && !name_it->second.empty())
            ap->name = name_it->second;
        if (city_it != form.end() && !city_it->second.empty())
            ap->city = city_it->second;
        if (country_it != form.end() && !country_it->second.empty())
            ap->country = country_it->second;

        return crow::response(successPage("Airport modified successfully!"));
    });

    // Airport – DELETE (HTML response)
    CROW_ROUTE(app, "/manage/airport/delete").methods("POST"_method)
    ([](const crow::request& req) {
        auto form = parseFormBody(req.body);
        auto iata_it = form.find("iata");

        if (iata_it == form.end() || iata_it->second.empty())
            return crow::response(errorPage("Missing IATA parameter."));

        std::string iata = iata_it->second;
        std::transform(iata.begin(), iata.end(), iata.begin(), ::toupper);

        auto it = session_airports_by_iata.find(iata);
        if (it == session_airports_by_iata.end())
            return crow::response(errorPage("Airport not found."));

        int id = it->second->id;

        session_airports_by_iata.erase(iata);
        session_airports_by_id.erase(id);

        session_routes.erase(
            std::remove_if(
                session_routes.begin(),
                session_routes.end(),
                [&](const std::shared_ptr<Route>& r) {
                    return r->source_airport == iata ||
                           r->dest_airport   == iata;
                }
            ),
            session_routes.end()
        );

        return crow::response(successPage("Airport and all related routes deleted."));
    });

    // Route – INSERT
    CROW_ROUTE(app, "/manage/route/insert").methods("POST"_method)
    ([](const crow::request& req) {
        auto form = parseFormBody(req.body);

        auto airline_it = form.find("airline");
        auto source_it  = form.find("source");
        auto dest_it    = form.find("dest");

        if (airline_it == form.end() || source_it == form.end() ||
            dest_it == form.end() ||
            airline_it->second.empty() ||
            source_it->second.empty() ||
            dest_it->second.empty()) {
            return crow::response(errorPage("Missing required parameters."));
        }

        std::string airline = airline_it->second;
        std::string source  = source_it->second;
        std::string dest    = dest_it->second;

        std::transform(airline.begin(), airline.end(), airline.begin(), ::toupper);
        std::transform(source.begin(),  source.end(),  source.begin(),  ::toupper);
        std::transform(dest.begin(),    dest.end(),    dest.begin(),    ::toupper);

        if (!session_airlines_by_iata.count(airline))
            return crow::response(errorPage("Airline not found."));

        if (!session_airports_by_iata.count(source) || !session_airports_by_iata.count(dest))
            return crow::response(errorPage("Source or destination airport not found."));

        auto r = std::make_shared<Route>();
        r->airline_code   = airline;
        r->source_airport = source;
        r->dest_airport   = dest;
        r->stops          = 0;

        session_routes.push_back(r);

        return crow::response(successPage("Route inserted successfully!"));
    });

    // Route – DELETE
    CROW_ROUTE(app, "/manage/route/delete").methods("POST"_method)
    ([](const crow::request& req) {
        auto form = parseFormBody(req.body);

        auto airline_it = form.find("airline");
        auto source_it  = form.find("source");
        auto dest_it    = form.find("dest");

        if (airline_it == form.end() || source_it == form.end() ||
            dest_it == form.end() ||
            airline_it->second.empty() ||
            source_it->second.empty() ||
            dest_it->second.empty()) {
            return crow::response(errorPage("Missing required parameters."));
        }

        std::string airline = airline_it->second;
        std::string source  = source_it->second;
        std::string dest    = dest_it->second;

        std::transform(airline.begin(), airline.end(), airline.begin(), ::toupper);
        std::transform(source.begin(),  source.end(),  source.begin(),  ::toupper);
        std::transform(dest.begin(),    dest.end(),    dest.begin(),    ::toupper);

        size_t before = session_routes.size();

        session_routes.erase(
            std::remove_if(
                session_routes.begin(),
                session_routes.end(),
                [&](const std::shared_ptr<Route>& r) {
                    return r->airline_code == airline &&
                           r->source_airport == source &&
                           r->dest_airport   == dest;
                }
            ),
            session_routes.end()
        );

        if (session_routes.size() == before)
            return crow::response(errorPage("No matching route found."));

        return crow::response(successPage("Route deleted successfully!"));
    });

    std::cout << "OpenFlights Web Service Starting...\n";
    std::cout << "Server running on http://localhost:8080\n";
    std::cout << "Press Ctrl+C to stop\n";
    
    app.port(8080).multithreaded().run();
    
    return 0;
}