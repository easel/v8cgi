var assert = require("assert");
var util = require("util").Util;

exports.testBase64 = function() {
	var s1 = "hello";
	var s2 = "aGVsbG8=";
	assert.assertEquals(s2, util.base64encode(s1), "base64 encode");
	assert.assertEquals(s1, util.base64decode(s2), "base64 decode");
}

exports.testSha1 = function() {
	var s1 = "hello";
	var s2 = "aaf4c61ddcc5e8a2dabede0f3b482cd9aea9434d";
	assert.assertEquals(s2, util.sha1(s1), "sha1");
}

exports.testMd5 = function() {
	var s1 = "hello";
	var s2 = "5d41402abc4b2a76b9719d911017c592";
	assert.assertEquals(s2, util.md5(s1), "md5");
}

exports.testUtf8 = function() {
	var s1 = "žšČ";
	var s2 = String.fromCharCode(197, 190, 197, 161, 196, 140);
	assert.assertEquals(s2, util.utf8decode(s1), "utf8 decode");
	assert.assertEquals(s1, util.utf8encode(s2), "utf8 encode");
}

exports.testJson = function() {
	var data = [1,"2",{"\n":[]},null,true];
	var str = "[1,\"2\",{\"\\n\":[]},null,true]";
	assert.assertEquals(str, util.serialize(data), "json serialize");
	assert.assertEquals(str, util.serialize(util.deserialize(str)), "json deserialize");
	
	var a = [];
	a.push(a);
	assert.assertThrows(function(){util.serialize(a);}, "cyclic structure");
	assert.assertThrows(function(){util.serialize(function(){});}, "function");
}