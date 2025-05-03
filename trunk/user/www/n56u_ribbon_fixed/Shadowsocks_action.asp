<!DOCTYPE html>
<html>
<head>
<title></title>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<meta http-equiv="Pragma" content="no-cache">
<meta http-equiv="Expires" content="-1">

<script>

var restart_time = 1;

function restart_needed_time(second){
	restart_time = second;
}

function Callback(){
	window.parent.document.documentElement.style.overflowY = 'visible';
}
</script>
</head>

<body onLoad="Callback();">
<% shadowsocks_action(); %>

</form>

</body>
</html>
