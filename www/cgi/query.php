<?php
echo "<html><body>";
echo "<h1>Query Test</h1>";
echo "<p>name = " . (isset($_GET['name']) ? $_GET['name'] : "yok") . "</p>";
echo "<p>QUERY_STRING = " . getenv('QUERY_STRING') . "</p>";
echo "</body></html>";
?>
