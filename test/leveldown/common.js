const test = require('tape')
const tempy = require('tempy')
const leveldown = require('../../dist/leveldown')
const suite = require('abstract-leveldown/test')

module.exports = suite.common({
  test: test,
  factory: function () {
    return leveldown(tempy.directory())
  },

  // Opt-in to new tests
  clear: true,
  getMany: true
})
