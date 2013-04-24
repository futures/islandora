<?php

/**
 * @file
 * This administration form for this module.
 */

/**
 * The administration form for the fedora repository connection.
 *
 * @param array $form
 *   The Drupal form definition.
 * @param array $form_state
 *   The Drupal form state.
 *
 * @return array
 *   The Drupal form definition.
 */
function islandora_repository_admin(array $form, array &$form_state) {
  module_load_include('inc', 'islandora', 'includes/utilities');
  drupal_add_css(drupal_get_path('module', 'islandora') . '/css/islandora.admin.css');

  $form = array();
  if (isset($form_state['values']['islandora_base_url'])) {
    $url = $form_state['values']['islandora_base_url'];
  }
  else {
    $url = variable_get('islandora_base_url', 'http://localhost:8080/fedora');
  }

  $connection = islandora_get_tuque_connection(NULL, $url);
  if (!$connection) {
    return;
  }

  try {
    $info = $connection->api->a->describeRepository();
  } catch (Exception $e) {
    $info = FALSE;
  }
  global $base_url;
  $events_feed = '<a href="' . $base_url . '/islandora/events">Events Feed</a>';

  if (isset($info) && !empty($info)) {
    $number_of_objects = (!isset($info['numberOfObjects'])) ? 'unknown' : $info['numberOfObjects'];
    $repository_version = (!isset($info['repositoryVersion'])) ? 'unknown' : $info['repositoryVersion'];
    $repository_size = (!isset($info['repositorySize'])) ? 'unknown' : $info['repositorySize'];
    $confirmation_message = '<img src="' . url('misc/watchdog-ok.png') . '"/>'
        . t('Successfully connected to Fedora Server (Version !version) with a total of !objects objects and a total size of !size_of', array('!version' => $repository_version
          , '!objects' => $number_of_objects, '!size_of' => format_size($repository_size)));

    // attempt to register the default namespace
    try {
      $response = $connection->api->m->registerNamespace("islandora", "http://islandora.ca");
      if ($response && $response['status'] == 201) {
        $msg = '<img src="' . url('misc/watchdog-ok.png') . '"/>' . t("Successfully registered default islandora namespace");
        $msg = $msg . '<br /><br />' . $events_feed;
      }
      else {
        $msg = t("Encountered error @error ocurred during default namespace registration", array("@error" =>
          $response['status']));
      }
    } catch (RepositoryException $e) {
      $msg = t("Failed to register default islandora namespace");
    }
    $confirmation_message .= "<br />" . $msg;
  }
  else {
    $confirmation_message = theme_image(array('path' => 'misc/watchdog-error.png', 'attributes' => array()));
    $confirmation_message .= t('Unable to connect to Fedora server at !islandora_url', array(
      '!islandora_url' => $url));
  }

  $form['islandora_tabs'] = array(
    '#type' => 'vertical_tabs',
  );

  $form['islandora_tabs']['islandora_general'] = array(
    '#type' => 'fieldset',
    '#title' => t('General Configuarion'),
  );

  // Ajax wrapper for url checking.
  $form['islandora_tabs']['islandora_general']['wrapper'] = array(
    '#prefix' => '<div id="islandora-url">',
    '#suffix' => '</div>',
    '#type' => 'markup',
  );

  $form['islandora_tabs']['islandora_general']['wrapper']['islandora_base_url'] = array(
    '#type' => 'textfield',
    '#title' => t('Fedora base URL'),
    '#default_value' => variable_get('islandora_base_url', 'http://localhost:8080/fedora'),
    '#description' => t('The URL to use for REST connections <br /> !confirmation_message', array(
      '!confirmation_message' => $confirmation_message,
    )),
    '#required' => TRUE,
    '#ajax' => array(
      'callback' => 'islandora_update_url_div',
      'wrapper' => 'islandora-url',
      'effect' => 'fade',
      'event' => 'blur',
      'progress' => array('type' => 'throbber'),
    ),
  );

  $form['islandora_tabs']['islandora_general']['islandora_repository_pid'] = array(
    '#type' => 'textfield',
    '#title' => t('Root Collection PID'),
    '#default_value' => variable_get('islandora_repository_pid', 'islandora:root'),
    '#description' => t('The PID of the Root Collection Object'),
    '#required' => TRUE,
  );

  $form['islandora_tabs']['islandora_namespace'] = array(
    '#type' => 'fieldset',
    '#title' => t('Namespaces'),
  );

  $form['islandora_tabs']['islandora_namespace']['wrapper'] = array(
    '#type' => 'markup',
    '#prefix' => '<div id="islandora-namespace">',
    '#suffix' => '</div>',
  );

  $form['islandora_tabs']['islandora_namespace']['wrapper']['islandora_namespace_restriction_enforced'] = array(
    '#weight' => -1,
    '#type' => 'checkbox',
    '#title' => t('Enforce namespace restrictions'),
    '#description' => t("Allow administrator to restrict user's access to the PID namepaces listed below"),
    '#default_value' => variable_get('islandora_namespace_restriction_enforced', FALSE),
    '#ajax' => array(
      'callback' => 'islandora_update_namespace_div',
      'wrapper' => 'islandora-namespace',
      'effect' => 'fade',
      'event' => 'change',
      'progress' => array('type' => 'throbber'),
    ),
  );
  if (isset($info) && !empty($info)) {
    try{
      $ns_json = $connection->api->m->getRegisteredNamespaces();
      $namespaces = parseNamespace($ns_json);
     } catch (Exception $e){
      drupal_set_message("error retrieving namespaces");
      $namespaces == NULL;
    }
    if (isset($namespaces)) {
      $form['islandora_tabs']['islandora_namespace']['nslist'] = array(
        '#title' => 'Registered Namespaces',
        '#markup' => $namespaces,
      );

      if (isset($form_state['values']['islandora_namespace_restriction_enforced'])) {
        $namespaces = $form_state['values']['islandora_namespace_restriction_enforced'];
      }
      else {
        $namespaces = variable_get('islandora_namespace_restriction_enforced', FALSE);
      }

      if ($namespaces) {
        $form['islandora_tabs']['islandora_namespace']['wrapper']['islandora_pids_allowed'] = array(
          '#type' => 'textfield',
          '#title' => t('PID namespaces allowed in this Drupal install'),
          '#default_value' => variable_get('islandora_pids_allowed', 'default: demo: changeme: ilives: islandora-book: books: newspapers: '),
          '#description' => t('A space separated list of PID namespaces that users are permitted to access from this Drupal installation. <br /> This could be more than a simple namespace ie demo:mydemos. <br /> islandora: is reserved and is always allowed.'),
          '#weight' => 0,
        );
      }
    }
  }
    return system_settings_form($form);
  }

  /**
   * Get the element to render for the AJAX event that triggered this callback.
   *
   * @param array $form
   *   The Drupal form definition.
   * @param array $form_state
   *   The Drupal form state.
   *
   * @return array
   *   The form element to render.
   */
  function islandora_update_url_div(array $form, array $form_state) {
    return $form['islandora_tabs']['islandora_general']['wrapper'];
  }

  /**
   * Get the element to render for the AJAX event that triggered this callback.
   *
   * @param array $form
   *   The Drupal form definition.
   * @param array $form_state
   *   The Drupal form state.
   *
   * @return array
   *   The form element to render.
   */
  function islandora_update_namespace_div(array $form, array $form_state) {
    return $form['islandora_tabs']['islandora_namespace']['wrapper'];
  }

  /**
   * Format the namespace list given into displayable text.  Currently we just
   * wrap the keys/values into a table and return the markup.
   */
  function parseNamespace($ns_json) {
    $nsmap = json_decode($ns_json);
    if ($nsmap !== NULL) {
      $nsmap = $nsmap->namespaceRegistry->namespace;
    }
    else {
      return "Error parsing namespace list";
    }
    $map = "<table>";
    $key = "@prefix";
    $value = "@URI";
    foreach ($nsmap as $kv) {
      if ($kv->$key !== "") {
        $map = $map . "<tr><td>" . $kv->$key . "</td><td>" . $kv->$value . "</td></tr>";
      }
    }
    return $map . "</table>";
  }
