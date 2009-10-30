/**
 * This file tests the JS module
 */

var assert = require("assert");
require("js");

exports.testString = function() {
	assert.assertEquals("lpad without arguments", "03", "3".lpad());
	assert.assertEquals("redundant lpad without arguments", "333", "333".lpad());
	assert.assertEquals("lpad with arguments", "xx3", "3".lpad("x", 3));

	assert.assertEquals("rpad without arguments", "30", "3".rpad());
	assert.assertEquals("redundant rpad without arguments", "333", "333".rpad());
	assert.assertEquals("rpad with arguments", "3xx", "3".rpad("x", 3));
	
	assert.assertEquals("trim", "hello \r\n dolly", "  \n hello \r\n dolly\t".trim());
	assert.assertEquals("redundant trim", "hello dolly", "hello dolly".trim());
}

exports.testDate = function() {
	var d = new Date();
	d.setMilliseconds(0);
	d.setSeconds(1);
	d.setMinutes(2);
	d.setHours(3);
	d.setDate(4);
	d.setMonth(0);
	d.setFullYear(2006);
	
	var result = d.format("! d j N S w z W m n t L Y y a A g G h H i s U");
	var ts = 1136343721 + d.getTimezoneOffset()*60;
	var str = "! 04 4 3 th 3 3 01 01 1 31 0 2006 06 am AM 3 3 03 03 02 01 "+ts;
	assert.assertEquals("date format", str, result);
}
