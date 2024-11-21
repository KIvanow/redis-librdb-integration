// Required Node.js addon development headers
#include <napi.h>
#include <iostream>
#include <sstream>
#include <string>
#include <fstream>
#include <vector>

// Handle naming conflict between Node.js and librdb
#define delete delete_func
#include <librdb/librdb-api.h>
#include <librdb/librdb-ext-api.h>
#undef delete

// Structure to store Redis key types during parsing
struct KeyTypeInfo {
    std::string key;      // Redis key name
    int dataType;         // Redis data type (0=string, 1=list, etc.)
};

/**
 * Generates a JSON array containing all Redis database information
 * Format: [
 *   {"__aux__": {...}},        // Redis metadata (version, memory usage, etc.)
 *   {"__dbsize__": {...}},     // Database statistics
 *   {key-value pairs...},      // Actual Redis data
 *   {"__types__": {...}}       // Data type information for each key
 * ]
 */
std::string generateJson(const std::string& auxInfo, const std::string& dbSizeInfo, 
                        const std::string& keyValuePairs, const std::string& typeInfo) {
    std::vector<std::string> jsonObjects;
    
    // Add each non-empty section as a JSON object
    if (!auxInfo.empty()) {
        jsonObjects.push_back("{\"__aux__\":" + auxInfo + "}");
    }
    if (!dbSizeInfo.empty()) {
        jsonObjects.push_back("{\"__dbsize__\":" + dbSizeInfo + "}");
    }
    if (!keyValuePairs.empty()) {
        jsonObjects.push_back("{" + keyValuePairs + "}");
    }
    if (!typeInfo.empty()) {
        jsonObjects.push_back("{\"__types__\":" + typeInfo + "}");
    }

    // Combine all objects with commas and wrap in array brackets
    std::ostringstream jsonData;
    jsonData << "[";
    
    for (size_t i = 0; i < jsonObjects.size(); ++i) {
        if (i > 0) jsonData << ",";
        jsonData << jsonObjects[i];
    }
    
    jsonData << "]";
    
    return jsonData.str();
}

/**
 * Main Node.js addon function that parses Redis RDB files
 * @param info Contains the file path to the RDB file
 * @return JSON string containing all parsed Redis data
 */
Napi::Value ParseRDB(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    try {
        // Validate input arguments
        if (info.Length() < 1) {
            throw Napi::Error::New(env, "Wrong number of arguments");
        }

        // Get file path and setup temporary storage
        std::string filepath = info[0].As<Napi::String>().Utf8Value();
        std::string tempFile = "temp_output.json";  // Temporary file for JSON output
        std::vector<KeyTypeInfo> keyTypes;          // Store type information during parsing

        // Configure Redis parser options
        RdbxToJsonConf jsonConf = {
            .level = RDB_LEVEL_DATA,            // Parse all data
            .encoding = RDBX_CONV_JSON_ENC_PLAIN, // Use plain JSON encoding
            .includeDbInfo = 1,                 // Include database statistics
            .includeAuxField = 1,               // Include Redis metadata
            .includeFunc = 0,                   // Skip Redis functions
            .includeStreamMeta = 0,             // Skip stream metadata
            .flatten = 0                        // Don't flatten JSON structure
        };

        // Initialize Redis parser
        RdbParser *parser = RDB_createParserRdb(NULL);
        if (!parser) {
            throw Napi::Error::New(env, "Failed to create RDB parser");
        }

        // Set logging level to errors only
        RDB_setLogLevel(parser, RDB_LOG_ERR);

        // Open the RDB file for reading
        if (!RDBX_createReaderFile(parser, filepath.c_str())) {
            RDB_deleteParser(parser);
            throw Napi::Error::New(env, "Failed to create reader file");
        }

        // Setup callback to collect type information for each Redis key
        RdbHandlersDataCallbacks callbacks = {};
        callbacks.handleNewKey = [](RdbParser *p, void *userData, RdbBulk key, RdbKeyInfo *info) {
            auto types = static_cast<std::vector<KeyTypeInfo>*>(userData);
            types->push_back(KeyTypeInfo{std::string(key), info->dataType});
            return RDB_OK;
        };

        // Create handlers for processing the RDB data and collecting type information
        RdbHandlers* dataHandlers = RDB_createHandlersData(parser, &callbacks, &keyTypes, nullptr);
        if (!dataHandlers) {
            RDB_deleteParser(parser);
            throw Napi::Error::New(env, "Failed to create data handlers");
        }

        // Setup JSON output handler
        if (!RDBX_createHandlersToJson(parser, tempFile.c_str(), &jsonConf)) {
            RDB_deleteParser(parser);
            throw Napi::Error::New(env, "Failed to create JSON handlers");
        }

        // Parse the RDB file
        RdbStatus status;
        while ((status = RDB_parse(parser)) == RDB_STATUS_WAIT_MORE_DATA);

        // Check for parsing errors
        if (status != RDB_STATUS_OK) {
            const char* error = RDB_getErrorMessage(parser);
            RDB_deleteParser(parser);
            throw Napi::Error::New(env, error ? error : "Failed to parse RDB file");
        }

        RDB_deleteParser(parser);

        // Read the generated JSON file
        std::ifstream inFile(tempFile);
        if (!inFile.is_open()) {
            throw Napi::Error::New(env, "Failed to open output JSON file");
        }

        // Load JSON content into memory
        std::stringstream buffer;
        buffer << inFile.rdbuf();
        std::string jsonData = buffer.str();
        inFile.close();
        std::remove(tempFile.c_str());  // Clean up temporary file

        // Extract individual sections from the JSON
        std::string auxInfo, dbSizeInfo, keyValuePairs;
        size_t auxStart = jsonData.find("\"__aux__\":");
        size_t dbSizeStart = jsonData.find("\"__dbsize__\":");
        
        // Extract auxiliary information section
        if (auxStart != std::string::npos) {
            size_t auxObjStart = jsonData.find("{", auxStart);
            size_t auxObjEnd = jsonData.find("}", auxObjStart) + 1;
            auxInfo = jsonData.substr(auxObjStart, auxObjEnd - auxObjStart);
        }
        
        // Extract database size information section
        if (dbSizeStart != std::string::npos) {
            size_t dbSizeObjStart = jsonData.find("{", dbSizeStart);
            size_t dbSizeObjEnd = jsonData.find("}", dbSizeObjStart) + 1;
            dbSizeInfo = jsonData.substr(dbSizeObjStart, dbSizeObjEnd - dbSizeObjStart);
        }

        // Extract key-value pairs section
        if (dbSizeStart != std::string::npos) {
            size_t kvStart = jsonData.find("}", dbSizeStart) + 1;
            size_t kvEnd = jsonData.rfind("}");
            keyValuePairs = jsonData.substr(kvStart, kvEnd - kvStart);
            // Clean up any trailing/leading commas
            if (keyValuePairs.front() == ',') keyValuePairs = keyValuePairs.substr(1);
            if (keyValuePairs.back() == ',') keyValuePairs = keyValuePairs.substr(0, keyValuePairs.length() - 1);
        }

        // Generate types information section
        std::string typeJson = "{";
        bool first = true;
        for (const auto& type : keyTypes) {
            if (!first) typeJson += ",";
            first = false;
            typeJson += "\"" + type.key + "\":" + std::to_string(type.dataType);
        }
        typeJson += "}";

        // Combine all sections into final JSON structure
        jsonData = generateJson(auxInfo, dbSizeInfo, keyValuePairs, typeJson);

        // Return the JSON string to Node.js
        return Napi::String::New(env, jsonData);
    } catch (const std::exception& e) {
        throw Napi::Error::New(env, e.what());
    }
}

/**
 * Initialize the Node.js addon
 * Exports the ParseRDB function to be called from JavaScript
 */
Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set(Napi::String::New(env, "parseRDB"),
                Napi::Function::New(env, ParseRDB));
    return exports;
}

// Register the addon with Node.js
NODE_API_MODULE(addon, Init)