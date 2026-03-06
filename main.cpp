// #include "Analyzer/Analyzer.h"

// int main() {
//     while(true){
//         Analyzer::Analyze();
//     }
// }

#include "Analyzer/Analyzer.h"
#include "Libs/httplib.h"
#include <iostream>
#include <sstream>
#include <string>

int main()
{
    httplib::Server svr;

    // ================= CORS =================
    svr.set_pre_routing_handler([](const httplib::Request& req,
                                   httplib::Response& res)
    {
        res.set_header("Access-Control-Allow-Origin",  "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        if(req.method == "OPTIONS")
        {
            res.status = 204;
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });

    // ================= POST /execute =================
    // Recibe: { "commands": "mkdisk ...\nfdisk ...\n" }
    // Retorna: { "output": "resultado..." }
    svr.Post("/execute", [](const httplib::Request& req,
                             httplib::Response& res)
    {
        // Parsear JSON manualmente (sin dependencias)
        std::string body = req.body;
        std::string commands = "";

        // Extraer valor de "commands"
        auto pos = body.find("\"commands\"");
        if(pos != std::string::npos)
        {
            auto start = body.find("\"", pos + 10);
            if(start != std::string::npos)
            {
                start++; // saltar la comilla de apertura
                std::string result = "";
                bool escaped = false;
                for(size_t i = start; i < body.size(); i++)
                {
                    char c = body[i];
                    if(escaped)
                    {
                        if(c == 'n')       result += '\n';
                        else if(c == 't')  result += '\t';
                        else if(c == '"')  result += '"';
                        else if(c == '\\') result += '\\';
                        else               result += c;
                        escaped = false;
                    }
                    else if(c == '\\')
                    {
                        escaped = true;
                    }
                    else if(c == '"')
                    {
                        break; // fin del string
                    }
                    else
                    {
                        result += c;
                    }
                }
                commands = result;
            }
        }

        if(commands.empty())
        {
            res.set_content("{\"output\":\"Error: No se recibieron comandos\"}", 
                           "application/json");
            return;
        }

        // Capturar stdout
        std::ostringstream output;
        std::streambuf* oldCout = std::cout.rdbuf(output.rdbuf());

        // Ejecutar línea por línea
        std::istringstream iss(commands);
        std::string line;
        while(std::getline(iss, line))
        {
            // Ignorar líneas vacías y comentarios
            if(line.empty()) continue;

            // Trim espacios al inicio
            size_t first = line.find_first_not_of(" \t\r\n");
            if(first == std::string::npos) continue;
            line = line.substr(first);

            // Mostrar comentarios tal cual
            if(line[0] == '#')
            {
                std::cout << line << "\n";
                continue;
            }

            Analyzer::AnalyzeLine(line);
        }

        // Restaurar stdout
        std::cout.rdbuf(oldCout);

        // Escapar output para JSON
        std::string raw = output.str();
        std::string escaped = "";
        for(char c : raw)
        {
            if(c == '"')       escaped += "\\\"";
            else if(c == '\\') escaped += "\\\\";
            else if(c == '\n') escaped += "\\n";
            else if(c == '\r') escaped += "\\r";
            else if(c == '\t') escaped += "\\t";
            else               escaped += c;
        }

        std::string json = "{\"output\":\"" + escaped + "\"}";
        res.set_content(json, "application/json");
    });

    // ================= GET /ping =================
    svr.Get("/ping", [](const httplib::Request&, httplib::Response& res)
    {
        res.set_content("{\"status\":\"ok\"}", "application/json");
    });

    std::cout << "Servidor corriendo en http://localhost:8080\n";
    svr.listen("0.0.0.0", 8080);

    return 0;
}
