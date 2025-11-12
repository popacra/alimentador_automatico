<?php
// Configuración de la base de datos
$host = 'db.tecnica4berazategui.edu.ar';        // Cambia por tu host
$dbname = 'jmyedro_crocmasterdb';      // Cambia por tu base de datos
$username = 'jmyedro_crocmasteru';   // Cambia por tu usuario
$password = 'W4ZT0s2eSKci5i1#X)Hk*rsZ/@K63uJcwaZ$X!#h9;OH)kzB';  // Cambia por tu contraseña

// Headers para CORS y JSON
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: GET, POST, OPTIONS');
header('Access-Control-Allow-Headers: Content-Type');
header('Content-Type: application/json; charset=utf-8');

// Manejar preflight requests
if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(200);
    exit();
}

try {
    $pdo = new PDO("mysql:host=$host;dbname=$dbname;charset=utf8mb4", $username, $password);
    $pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
    $pdo->setAttribute(PDO::ATTR_DEFAULT_FETCH_MODE, PDO::FETCH_ASSOC);
} catch (PDOException $e) {
    // Devolver error en formato JSON
    http_response_code(500);
    echo json_encode([
        'success' => false,
        'message' => 'Error de conexión a la base de datos',
        'error' => $e->getMessage()
    ]);
    exit();
}
?>