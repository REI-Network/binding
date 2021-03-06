const make = require('./make')

// This test isn't included in abstract-leveldown because
// the empty-check is currently performed by leveldown.
make('iterator#seek throws if target is empty', function (db, t, done) {
  const targets = ['', Buffer.alloc(0), []]
  let pending = targets.length

  targets.forEach(function (target) {
    const ite = db.iterator()
    let error

    try {
      ite.seek(target)
    } catch (err) {
      error = err.message
    }

    t.is(error, 'cannot seek() to an empty target', 'got error')
    ite.end(end)
  })

  function end (err) {
    t.ifError(err, 'no error from end()')
    if (!--pending) done()
  }
})

make('iterator optimized for seek', function (db, t, done) {
  const batch = db.batch()
  batch.put('a', 1)
  batch.put('b', 1)
  batch.put('c', 1)
  batch.put('d', 1)
  batch.put('e', 1)
  batch.put('f', 1)
  batch.put('g', 1)
  batch.write(function (err) {
    const ite = db.iterator()
    t.ifError(err, 'no error from batch()')
    ite.next(function (err, key, value) {
      t.ifError(err, 'no error from next()')
      t.equal(key.toString(), 'a', 'key matches')
      t.equal(ite.cache.length, 0, 'no cache')
      ite.next(function (err, key, value) {
        t.ifError(err, 'no error from next()')
        t.equal(key.toString(), 'b', 'key matches')
        t.ok(ite.cache.length > 0, 'has cached items')
        ite.seek('d')
        t.notOk(ite.cache, 'cache is removed')
        ite.next(function (err, key, value) {
          t.ifError(err, 'no error from next()')
          t.equal(key.toString(), 'd', 'key matches')
          t.equal(ite.cache.length, 0, 'no cache')
          ite.next(function (err, key, value) {
            t.ifError(err, 'no error from next()')
            t.equal(key.toString(), 'e', 'key matches')
            t.ok(ite.cache.length > 0, 'has cached items')
            ite.end(done)
          })
        })
      })
    })
  })
})

make('close db with open iterator', function (db, t, done) {
  const ite = db.iterator()
  let cnt = 0
  let hadError = false

  ite.next(function loop (err, key, value) {
    if (cnt++ === 0) {
      // The first call should succeed, because it was scheduled before close()
      t.ifError(err, 'no error from next()')
    } else {
      // The second call should fail, because it was scheduled after close()
      t.equal(err.message, 'iterator has ended')
      hadError = true
    }
    if (key !== undefined) { ite.next(loop) }
  })

  db.close(function (err) {
    t.ifError(err, 'no error from close()')
    t.ok(hadError)

    done(null, false)
  })
})

make('key-only iterator', function (db, t, done) {
  const it = db.iterator({ values: false, keyAsBuffer: false, valueAsBuffer: false })

  it.next(function (err, key, value) {
    t.ifError(err, 'no next() error')
    t.is(key, 'one')
    t.is(value, '') // should this be undefined?
    it.end(done)
  })
})

make('value-only iterator', function (db, t, done) {
  const it = db.iterator({ keys: false, keyAsBuffer: false, valueAsBuffer: false })

  it.next(function (err, key, value) {
    t.ifError(err, 'no next() error')
    t.is(key, '') // should this be undefined?
    t.is(value, '1')
    it.end(done)
  })
})
