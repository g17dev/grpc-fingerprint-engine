#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <glib.h>

#include "base64.h"
#include <grpc/grpc.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpcpp/security/server_credentials.h>
#include "helper.h"
#include "fingerprint.grpc.pb.h"
#include "libfprint_adapter.h"

#define MAX_PORT "65535"
#define DEFAULT_PORT "4134"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using fingerprint::EnrolledFMD;
using fingerprint::EnrollmentRequest;
using fingerprint::VerificationRequest;
using fingerprint::VerificationResponse;
using fingerprint::CheckDuplicateResponse;
using fingerprint::FingerPrint;

using std::vector;
using std::string;
using std::cout;
using std::endl;
using std::unique_ptr;

class FingerPrintImpl final : public FingerPrint::Service {
    public:
        Status EnrollFingerprint(ServerContext* context, const EnrollmentRequest* enrollmentRequest, EnrolledFMD* enrolledFMD) {
            std::string fmd = capture_to_fmd();
            if (fmd.empty()) {
                return Status(grpc::StatusCode::INTERNAL, "No se pudo capturar la huella");
            }
            enrolledFMD->set_base64enrolledfmd(fmd);
            return Status::OK;
        }

        Status VerifyFingerprint(ServerContext* context, const VerificationRequest* verification_request, VerificationResponse* verification_response) {
            std::string current_fmd = capture_to_fmd();
            if (current_fmd.empty()) {
                verification_response->set_match(false);
                return Status(grpc::StatusCode::INTERNAL, "No se pudo capturar la huella");
            }
            
            gsize current_len;
            guchar *current_data = g_base64_decode(current_fmd.c_str(), &current_len);
            if (!current_data || current_len < 100) {
                verification_response->set_match(false);
                return Status::OK;
            }
            
            for (const auto& candidate : verification_request->fmdcandidates()) {
                std::string stored_fmd = candidate.base64enrolledfmd();
                
                gsize stored_len;
                guchar *stored_data = g_base64_decode(stored_fmd.c_str(), &stored_len);
                if (!stored_data || stored_len < 100) continue;
                
                if (stored_len == current_len) {
                    int matches = 0;
                    int total_checks = 0;
                    
                    // Zona 1: primeros 100 bytes
                    for (int i = 0; i < 100 && i < (int)stored_len; i++) {
                        if (stored_data[i] == current_data[i]) matches++;
                        total_checks++;
                    }
                    
                    // Zona 2: bytes del medio
                    int mid = stored_len / 2;
                    for (int i = mid - 50; i < mid + 50 && i < (int)stored_len; i++) {
                        if (i >= 0 && stored_data[i] == current_data[i]) matches++;
                        total_checks++;
                    }
                    
                    // Zona 3: últimos 100 bytes
                    int start = stored_len - 100;
                    for (int i = start; i < (int)stored_len; i++) {
                        if (i >= 0 && stored_data[i] == current_data[i]) matches++;
                        total_checks++;
                    }
                    
                    double similitud = (double)matches / total_checks * 100.0;
                    cout << "Similitud: " << similitud << "% (" << matches << "/" << total_checks << ")" << endl;
                    
                    g_free(stored_data);
                    
                    if (similitud >= 70.0) {
                        cout << "✅ Match!" << endl;
                        verification_response->set_match(true);
                        g_free(current_data);
                        return Status::OK;
                    }
                } else {
                    g_free(stored_data);
                }
            }
            
            cout << "❌ No match" << endl;
            verification_response->set_match(false);
            g_free(current_data);
            return Status::OK;
        }

        Status CheckDuplicate(ServerContext* context, const VerificationRequest* verification_request, CheckDuplicateResponse* check_duplicate_response) {
            check_duplicate_response->set_isduplicate(false);
            return Status::OK;
        }
};

void RunServer() {
    const char * port = getenv("PORT");
    string  server_address("0.0.0.0:");
    if (port && strlen(port) <= strlen(MAX_PORT)) {
        server_address.append(port);
    } else {
        server_address.append(DEFAULT_PORT);
    }

    FingerPrintImpl service;
    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    unique_ptr<Server> server(builder.BuildAndStart());
    cout << "Server started, listening on " << server_address << endl;
    server->Wait();
}   

int main() {
    if (!fp_init_device()) {
        std::cerr << "No se pudo inicializar el lector" << std::endl;
        return 1;
    }
    RunServer();
}