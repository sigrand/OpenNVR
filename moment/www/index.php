<?php
echo "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
$lang = substr($_SERVER['HTTP_ACCEPT_LANGUAGE'], 0, 2);
switch ($lang) {
    case "ru":
        include("index.ru.html");
        break;        
    default:
        include("index.en.html");
        break;
}
?>

