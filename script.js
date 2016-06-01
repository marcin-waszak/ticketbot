//phantomjs
var webPage = require('webpage');
var page = webPage.create();
var system = require('system');

if(system.args.length < 2)
{
	phantom.exit();
}
else
{
	page.open(system.args[1], function (status) 
	{
		var content = page.content;
		console.log(content);
		phantom.exit();
	});
}