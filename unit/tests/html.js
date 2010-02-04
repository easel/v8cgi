/**
 * This file tests html HTML module.
 */

var assert = require("assert");
var html = require("html").HTML;

exports.testHtml = function() {
	var s1 = "<a href='a&b'>\"";
	var s2 = "&lt;a href=&apos;a&amp;b&apos;&gt;&quot;";
	assert.equal(html.escape(s1), s2, "html escape");
	assert.equal(html.unescape(s2), s1, "html unsecape");
}
