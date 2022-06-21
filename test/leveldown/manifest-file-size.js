'use strict'

const fs = require('fs')
const path = require('path')
const test = require('tape')
const testCommon = require('./common')

test('manifest file size should\'n exceed limit', function (t) {
  const db = testCommon.factory()

  db.open({ manifestFileMaxSize: 300 }, async function (err) {
    t.ifError(err, 'no open error')

    for (let i = 0; i < 700000; i++) {
      await new Promise((r, j) => {
        db.put(String(i), 'value', (err) => {
            err ? j(err) : r()
        })
      })
      if (i % 100000 === 0) {
        console.log((i * 100 / 700000).toFixed(2) + '%')
      }
    }

    let file;
    for (const _file of fs.readdirSync(db.location)) {
        if (_file.startsWith('MANIFEST-')) {
            file = _file
            break
        }
    }

    t.assert(file !== undefined, 'manifest file should exist')
    const stat = fs.statSync(path.join(db.location, file))
    t.assert(stat.size <= 300, 'manifest file size should be less than limit')
    t.end()
  })
})
