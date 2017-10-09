#include <httpd.h>
#include <http_config.h>
#include <http_log.h>
#include <mod_cache.h>

module AP_MODULE_DECLARE_DATA cache_status_module;

// TODO: Put these into shared memory.
static int hit = 0;
static int total = 0;

// TODO: Create cache_status handler that outputs current hit/miss ratio.

static int cache_status(cache_handle_t *h, request_rec *r, apr_table_t *headers, ap_cache_status_e status, const char *reason)
{
  ap_log_error(APLOG_MARK, APLOG_INFO, 0, r->server, "mod_cache_status go!");

  switch (status) {
    case AP_CACHE_HIT:
    case AP_CACHE_REVALIDATE: {
      hit++;
      break;
    }
    case AP_CACHE_MISS:
    case AP_CACHE_INVALIDATE: {
      break;
    }
  }

  total++;

  ap_log_error(APLOG_MARK, APLOG_INFO, 0, r->server, "Current hit/miss ratio %d/%d %f", hit, total, hit/(total*1.0));

  return 1;
}

static void register_hooks(apr_pool_t *p)
{
  cache_hook_cache_status(cache_status, NULL, NULL, APR_HOOK_REALLY_LAST);
}

AP_DECLARE_MODULE( cache_status ) =
{
  STANDARD20_MODULE_STUFF,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  register_hooks,
};
