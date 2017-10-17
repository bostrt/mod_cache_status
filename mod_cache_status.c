#include <httpd.h>
#include <http_config.h>
#include <http_log.h>
#include <mod_cache.h>
#include <apr_shm.h>
#include <apr.h>
#include <util_mutex.h>
#include <unixd.h>

module AP_MODULE_DECLARE_DATA cache_status_module;

apr_shm_t *status_shm;

typedef struct status_data {
  int hit;
  int total;
} status_data;

apr_global_mutex_t *shm_mutex;

static int cache_status_post_config(apr_pool_t *pconf, apr_pool_t *plog, apr_pool_t *ptemp, server_rec *s)
{
  void *data;
  const char *userdata_key = "cache_status_post_config";
  apr_status_t rs;
  status_data *sdata;
  
  ap_log_error(APLOG_MARK, APLOG_INFO, 0, s, "mod_cache_status post config");

  apr_pool_userdata_get(&data, userdata_key, s->process->pool);
  if (!data) {
    // Do nothing on first call.
    apr_pool_userdata_set((const void *)1, userdata_key, apr_pool_cleanup_null, s->process->pool);
    return OK;
  }

  rs = apr_global_mutex_create(&shm_mutex, NULL, APR_LOCK_DEFAULT, pconf);
  
  if (rs != APR_SUCCESS) {
    ap_log_error(APLOG_MARK, APLOG_ERR, rs, s,
                 "Failed to create mutex lock");
    return HTTP_INTERNAL_SERVER_ERROR;    
  }

  ap_unixd_set_global_mutex_perms(shm_mutex);
  
  rs = apr_shm_create(&status_shm, sizeof(status_shm), NULL, pconf);
  if (rs != APR_SUCCESS) {
      ap_log_error(APLOG_MARK, APLOG_ERR, rs, s,
                   "Failed to create shared memory segment on file");
      return HTTP_INTERNAL_SERVER_ERROR;
  }

  sdata = (status_data *) apr_shm_baseaddr_get(status_shm);
  sdata->hit = 0;
  sdata->total = 0;

  return OK;
}

static void hook_child_init(apr_pool_t *mp, server_rec *s)
{
  apr_status_t rs = apr_global_mutex_child_init(&shm_mutex, NULL, mp);
  if (rs != APR_SUCCESS) {
    ap_log_error(APLOG_MARK, APLOG_ERR, rs, s,
                 "Failed to create shared memory segment on file during child initialization");    
  }
}

static int cache_status_hook_handler(cache_handle_t *h, request_rec *r, apr_table_t *headers, ap_cache_status_e status, const char *reason)
{
  apr_status_t rs;
  ap_log_error(APLOG_MARK, APLOG_INFO, 0, r->server, "mod_cache_status go!");
  rs = apr_global_mutex_lock(shm_mutex);
  if (rs != APR_SUCCESS) {
    ap_log_error(APLOG_MARK, APLOG_ERR, rs, r->server,
                 "Failed to acquire lock to update mod_cache_status.");    
    return !OK;
  }
  status_data *sdata = (status_data *) apr_shm_baseaddr_get(status_shm);

  switch (status) {
    case AP_CACHE_HIT:
    case AP_CACHE_REVALIDATE: {
      sdata->hit++;
      break;
    }
    case AP_CACHE_MISS:
    case AP_CACHE_INVALIDATE: {
      break;
    }
  }

  sdata->total++;

  ap_log_error(APLOG_MARK, APLOG_INFO, 0, r->server, "Current hit/miss ratio %d/%d %f", sdata->hit, sdata->total, sdata->hit/(sdata->total*1.0));

  apr_global_mutex_unlock(shm_mutex);
  return 1;
}

static void register_hooks(apr_pool_t *p)
{
  ap_hook_post_config(cache_status_post_config, NULL, NULL, APR_HOOK_MIDDLE);
  ap_hook_child_init(hook_child_init, NULL, NULL, APR_HOOK_MIDDLE);    
  cache_hook_cache_status(cache_status_hook_handler, NULL, NULL, APR_HOOK_REALLY_LAST);
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
