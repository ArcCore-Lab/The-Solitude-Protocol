<?php

$request_method = $_SERVER['REQUEST_METHOD'] ?? 'UNKNOWN';
$query_string = $_SERVER['QUERY_STRING'] ?? '';
$script_filename = $_SERVER['SCRIPT_FILENAME'] ?? '';

$response = "<!DOCTYPE html>\r\n";
$response .= "<html>\r\n";
$response .= "<head>\r\n";
$response .= "<meta charset=\"UTF-8\">\r\n";
$response .= "<title>CGI Test Results</title>\r\n";
$response .= "<style>\r\n";
$response .= "body { font-family: Consolas, monospace; margin: 20px; background: #f5f5f5; }\r\n";
$response .= "h1 { color: #333; border-bottom: 2px solid #0066cc; padding-bottom: 10px; }\r\n";
$response .= ".info { background: white; padding: 15px; margin: 10px 0; border-left: 4px solid #0066cc; }\r\n";
$response .= ".label { font-weight: bold; color: #0066cc; }\r\n";
$response .= ".value { color: #333; word-break: break-all; }\r\n";
$response .= "</style>\r\n";
$response .= "</head>\r\n";
$response .= "<body>\r\n";
$response .= "<h1>🔧 ArcCore CGI Test Report</h1>\r\n";

$response .= "<div class=\"info\">\r\n";
$response .= "<div><span class=\"label\">REQUEST_METHOD:</span> <span class=\"value\">" . htmlspecialchars($request_method) . "</span></div>\r\n";
$response .= "<div><span class=\"label\">QUERY_STRING:</span> <span class=\"value\">" . htmlspecialchars($query_string) . "</span></div>\r\n";
$response .= "<div><span class=\"label\">SCRIPT_FILENAME:</span> <span class=\"value\">" . htmlspecialchars($script_filename) . "</span></div>\r\n";
$response .= "</div>\r\n";

if (!empty($query_string)) {
    parse_str($query_string, $params);
    
    $response .= "<div class=\"info\">\r\n";
    $response .= "<div><span class=\"label\">📊 Query Parameters:</span></div>\r\n";
    
    foreach ($params as $key => $value) {
        $response .= "<div style=\"margin-left: 20px;\">" . htmlspecialchars($key) . " = " . htmlspecialchars($value) . "</div>\r\n";
    }
    
    $response .= "</div>\r\n";
}

$response .= "<div class=\"info\">\r\n";
$response .= "<div><span class=\"label\">PHP Version:</span> <span class=\"value\">" . phpversion() . "</span></div>\r\n";
$response .= "<div><span class=\"label\">Current Time:</span> <span class=\"value\">" . date('Y-m-d H:i:s') . "</span></div>\r\n";
$response .= "<div><span class=\"label\">Server Software:</span> <span class=\"value\">" . ($_SERVER['SERVER_SOFTWARE'] ?? 'N/A') . "</span></div>\r\n";
$response .= "</div>\r\n";

$response .= "<div class=\"info\">\r\n";
$response .= "<div><span class=\"label\">✅ CGI Execution Successful!</span></div>\r\n";
$response .= "</div>\r\n";

$response .= "</body>\r\n";
$response .= "</html>\r\n";

echo $response;
?>