# Copyright 2012 OpenStack Foundation
# All Rights Reserved.
#
#    Licensed under the Apache License, Version 2.0 (the "License"); you may
#    not use this file except in compliance with the License. You may obtain
#    a copy of the License at
#
#         http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
#    WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
#    License for the specific language governing permissions and limitations
#    under the License.

import copy
from oslo_log import log as logging

from automation.services.identity.v2.token_client import TokenClientJSON
from automation.services.identity.v3.token_client import V3TokenClientJSON

from automation.common import cred_provider
from automation.common import exceptions
from automation.common import negative_rest_client
from automation import config
from automation import manager

from automation.services.compute.json.agents_client import \
    AgentsClient
from automation.services.compute.json.aggregates_client import \
    AggregatesClient
from automation.services.compute.json.availability_zone_client import \
    AvailabilityZoneClient
from automation.services.compute.json.baremetal_nodes_client import \
    BaremetalNodesClient
from automation.services.compute.json.certificates_client import \
    CertificatesClient
from automation.services.compute.json.extensions_client import \
    ExtensionsClient
from automation.services.compute.json.fixed_ips_client import FixedIPsClient
from automation.services.compute.json.flavors_client import FlavorsClient
from automation.services.compute.json.floating_ip_pools_client import \
    FloatingIPPoolsClient
from automation.services.compute.json.floating_ips_bulk_client import \
    FloatingIPsBulkClient
from automation.services.compute.json.floating_ips_client import \
    FloatingIPsClient
from automation.services.compute.json.hosts_client import HostsClient
from automation.services.compute.json.hypervisor_client import \
    HypervisorClient
from automation.services.compute.json.images_client import ImagesClient
from automation.services.compute.json.instance_usage_audit_log_client import \
    InstanceUsagesAuditLogClient
from automation.services.compute.json.interfaces_client import \
    InterfacesClient
from automation.services.compute.json.keypairs_client import KeyPairsClient
from automation.services.compute.json.limits_client import LimitsClient
from automation.services.compute.json.migrations_client import \
    MigrationsClient
from automation.services.compute.json.networks_client import NetworksClient
from automation.services.compute.json.quota_classes_client import \
    QuotaClassesClient
from automation.services.compute.json.quotas_client import QuotasClient
from automation.services.compute.json.security_group_default_rules_client \
    import SecurityGroupDefaultRulesClient
from automation.services.compute.json.security_group_rules_client import \
    SecurityGroupRulesClient
from automation.services.compute.json.security_groups_client import \
    SecurityGroupsClient
from automation.services.compute.json.server_groups_client import \
    ServerGroupsClient
from automation.services.compute.json.servers_client import ServersClient
from automation.services.compute.json.services_client import ServicesClient
from automation.services.compute.json.tenant_networks_client import \
    TenantNetworksClient
from automation.services.compute.json.tenant_usages_client import \
    TenantUsagesClient
from automation.services.compute.json.volumes_extensions_client import \
    VolumesExtensionsClient

from automation.services.identity.v2.json.identity_client import \
    IdentityClient
from automation.services.identity.v3.json.credentials_client import \
    CredentialsClient
from automation.services.identity.v3.json.endpoints_client import \
    EndPointClient
from automation.services.identity.v3.json.identity_client import \
    IdentityV3Client
from automation.services.identity.v3.json.policy_client import PolicyClient
from automation.services.identity.v3.json.region_client import RegionClient
from automation.services.identity.v3.json.service_client import \
    ServiceClient
from automation.services.image.v1.json.image_client import ImageClient
from automation.services.image.v2.json.image_client import ImageClientV2
from automation.services.network.json.network_client import NetworkClient
from automation.services.volume.json.admin.volume_hosts_client import \
    VolumeHostsClient
from automation.services.volume.json.admin.volume_quotas_client import \
    VolumeQuotasClient
from automation.services.volume.json.admin.volume_services_client import \
    VolumesServicesClient
from automation.services.volume.json.admin.volume_types_client import \
    VolumeTypesClient
from automation.services.volume.json.availability_zone_client import \
    VolumeAvailabilityZoneClient
from automation.services.volume.json.backups_client import BackupsClient
from automation.services.volume.json.extensions_client import \
    ExtensionsClient as VolumeExtensionClient
from automation.services.volume.json.qos_client import QosSpecsClient
from automation.services.volume.json.snapshots_client import SnapshotsClient
from automation.services.volume.json.volumes_client import VolumesClient
from automation.services.volume.v2.json.admin.volume_hosts_client import \
    VolumeHostsV2Client
from automation.services.volume.v2.json.admin.volume_quotas_client import \
    VolumeQuotasV2Client
from automation.services.volume.v2.json.admin.volume_services_client import \
    VolumesServicesV2Client
from automation.services.volume.v2.json.admin.volume_types_client import \
    VolumeTypesV2Client
from automation.services.volume.v2.json.availability_zone_client import \
    VolumeV2AvailabilityZoneClient
from automation.services.volume.v2.json.backups_client import BackupsClientV2
from automation.services.volume.v2.json.extensions_client import \
    ExtensionsV2Client as VolumeV2ExtensionClient
from automation.services.volume.v2.json.qos_client import QosSpecsV2Client
from automation.services.volume.v2.json.snapshots_client import \
    SnapshotsV2Client
from automation.services.volume.v2.json.volumes_client import VolumesV2Client

CONF = config.CONF
LOG = logging.getLogger(__name__)


class Manager(manager.Manager):

    """
    Top level manager for OpenStack tempest clients
    """

    default_params = {
        'disable_ssl_certificate_validation':
            CONF.identity.disable_ssl_certificate_validation,
        'ca_certs': CONF.identity.ca_certificates_file,
        'trace_requests': CONF.debug.trace_requests
    }

    # NOTE: Tempest uses timeout values of compute API if project specific
    # timeout values don't exist.
    default_params_with_timeout_values = {
        'build_interval': CONF.compute.build_interval,
        'build_timeout': CONF.compute.build_timeout
    }
    default_params_with_timeout_values.update(default_params)

    def __init__(self, credentials=None, service=None):
        super(Manager, self).__init__(credentials=credentials)

        self._set_compute_clients()
        self._set_identity_clients()
        self._set_volume_clients()
        # self._set_object_storage_clients()

        self.network_client = NetworkClient(
            self.auth_provider,
            CONF.network.catalog_type,
            CONF.network.region or CONF.identity.region,
            endpoint_type=CONF.network.endpoint_type,
            build_interval=CONF.network.build_interval,
            build_timeout=CONF.network.build_timeout,
            **self.default_params)
        if CONF.service_available.glance:
            self.image_client = ImageClient(
                self.auth_provider,
                CONF.image.catalog_type,
                CONF.image.region or CONF.identity.region,
                endpoint_type=CONF.image.endpoint_type,
                build_interval=CONF.image.build_interval,
                build_timeout=CONF.image.build_timeout,
                **self.default_params)
            self.image_client_v2 = ImageClientV2(
                self.auth_provider,
                CONF.image.catalog_type,
                CONF.image.region or CONF.identity.region,
                endpoint_type=CONF.image.endpoint_type,
                build_interval=CONF.image.build_interval,
                build_timeout=CONF.image.build_timeout,
                **self.default_params)

        self.negative_client = negative_rest_client.NegativeRestClient(
            self.auth_provider, service, **self.default_params)

    def _set_compute_clients(self):
        params = {
            'service': CONF.compute.catalog_type,
            'region': CONF.compute.region or CONF.identity.region,
            'endpoint_type': CONF.compute.endpoint_type,
            'build_interval': CONF.compute.build_interval,
            'build_timeout': CONF.compute.build_timeout
        }
        params.update(self.default_params)

        self.agents_client = AgentsClient(self.auth_provider, **params)
        self.networks_client = NetworksClient(self.auth_provider, **params)
        self.migrations_client = MigrationsClient(self.auth_provider,
                                                  **params)
        self.security_group_default_rules_client = (
            SecurityGroupDefaultRulesClient(self.auth_provider, **params))
        self.certificates_client = CertificatesClient(self.auth_provider,
                                                      **params)
        self.servers_client = ServersClient(
            self.auth_provider,
            enable_instance_password=CONF.compute_feature_enabled
                .enable_instance_password,
            **params)
        self.server_groups_client = ServerGroupsClient(
            self.auth_provider, **params)
        self.limits_client = LimitsClient(self.auth_provider, **params)
        self.images_client = ImagesClient(self.auth_provider, **params)
        self.keypairs_client = KeyPairsClient(self.auth_provider, **params)
        self.quotas_client = QuotasClient(self.auth_provider, **params)
        self.quota_classes_client = QuotaClassesClient(self.auth_provider,
                                                       **params)
        self.flavors_client = FlavorsClient(self.auth_provider, **params)
        self.extensions_client = ExtensionsClient(self.auth_provider,
                                                  **params)
        self.floating_ip_pools_client = FloatingIPPoolsClient(
            self.auth_provider, **params)
        self.floating_ips_bulk_client = FloatingIPsBulkClient(
            self.auth_provider, **params)
        self.floating_ips_client = FloatingIPsClient(self.auth_provider,
                                                     **params)
        self.security_group_rules_client = SecurityGroupRulesClient(
            self.auth_provider, **params)
        self.security_groups_client = SecurityGroupsClient(
            self.auth_provider, **params)
        self.interfaces_client = InterfacesClient(self.auth_provider,
                                                  **params)
        self.fixed_ips_client = FixedIPsClient(self.auth_provider,
                                               **params)
        self.availability_zone_client = AvailabilityZoneClient(
            self.auth_provider, **params)
        self.aggregates_client = AggregatesClient(self.auth_provider,
                                                  **params)
        self.services_client = ServicesClient(self.auth_provider, **params)
        self.tenant_usages_client = TenantUsagesClient(self.auth_provider,
                                                       **params)
        self.hosts_client = HostsClient(self.auth_provider, **params)
        self.hypervisor_client = HypervisorClient(self.auth_provider,
                                                  **params)
        self.instance_usages_audit_log_client = \
            InstanceUsagesAuditLogClient(self.auth_provider, **params)
        self.tenant_networks_client = \
            TenantNetworksClient(self.auth_provider, **params)
        self.baremetal_nodes_client = BaremetalNodesClient(
            self.auth_provider, **params)

        # NOTE: The following client needs special timeout values because
        # the API is a proxy for the other component.
        params_volume = copy.deepcopy(params)
        params_volume.update({
            'build_interval': CONF.volume.build_interval,
            'build_timeout': CONF.volume.build_timeout
        })
        self.volumes_extensions_client = VolumesExtensionsClient(
            self.auth_provider, default_volume_size=CONF.volume.volume_size,
            **params_volume)

    def _set_identity_clients(self):
        params = {
            'service': CONF.identity.catalog_type,
            'region': CONF.identity.region
        }
        params.update(self.default_params_with_timeout_values)
        params_v2_admin = params.copy()
        params_v2_admin['endpoint_type'] = CONF.identity.v2_admin_endpoint_type
        # Client uses admin endpoint type of Keystone API v2
        self.identity_client = IdentityClient(self.auth_provider,
                                              **params_v2_admin)
        params_v2_public = params.copy()
        params_v2_public['endpoint_type'] = (
            CONF.identity.v2_public_endpoint_type)
        # Client uses public endpoint type of Keystone API v2
        self.identity_public_client = IdentityClient(self.auth_provider,
                                                     **params_v2_public)
        params_v3 = params.copy()
        params_v3['endpoint_type'] = CONF.identity.v3_endpoint_type
        # Client uses the endpoint type of Keystone API v3
        self.identity_v3_client = IdentityV3Client(self.auth_provider,
                                                   **params_v3)
        self.endpoints_client = EndPointClient(self.auth_provider,
                                               **params)
        self.service_client = ServiceClient(self.auth_provider, **params)
        self.policy_client = PolicyClient(self.auth_provider, **params)
        self.region_client = RegionClient(self.auth_provider, **params)
        self.credentials_client = CredentialsClient(self.auth_provider,
                                                    **params)
        # Token clients do not use the catalog. They only need default_params.
        # They read auth_url, so they should only be set if the corresponding
        # API version is marked as enabled
        if CONF.identity_feature_enabled.api_v2:
            if CONF.identity.uri:
                self.token_client = TokenClientJSON(
                    CONF.identity.uri, **self.default_params)
            else:
                msg = 'Identity v2 API enabled, but no identity.uri set'
                raise exceptions.InvalidConfiguration(msg)
        if CONF.identity_feature_enabled.api_v3:
            if CONF.identity.uri_v3:
                self.token_v3_client = V3TokenClientJSON(
                    CONF.identity.uri_v3, **self.default_params)
            else:
                msg = 'Identity v3 API enabled, but no identity.uri_v3 set'
                raise exceptions.InvalidConfiguration(msg)

    def _set_volume_clients(self):
        params = {
            'service': CONF.volume.catalog_type,
            'region': CONF.volume.region or CONF.identity.region,
            'endpoint_type': CONF.volume.endpoint_type,
            'build_interval': CONF.volume.build_interval,
            'build_timeout': CONF.volume.build_timeout
        }
        params.update(self.default_params)

        self.volume_qos_client = QosSpecsClient(self.auth_provider,
                                                **params)
        self.volume_qos_v2_client = QosSpecsV2Client(
            self.auth_provider, **params)
        self.volume_services_v2_client = VolumesServicesV2Client(
            self.auth_provider, **params)
        self.backups_client = BackupsClient(self.auth_provider, **params)
        self.backups_v2_client = BackupsClientV2(self.auth_provider,
                                                 **params)
        self.snapshots_client = SnapshotsClient(self.auth_provider,
                                                **params)
        self.snapshots_v2_client = SnapshotsV2Client(self.auth_provider,
                                                     **params)
        self.volumes_client = VolumesClient(
            self.auth_provider, default_volume_size=CONF.volume.volume_size,
            **params)
        self.volumes_v2_client = VolumesV2Client(
            self.auth_provider, default_volume_size=CONF.volume.volume_size,
            **params)
        self.volume_types_client = VolumeTypesClient(self.auth_provider,
                                                     **params)
        self.volume_services_client = VolumesServicesClient(
            self.auth_provider, **params)
        self.volume_hosts_client = VolumeHostsClient(self.auth_provider,
                                                     **params)
        self.volume_hosts_v2_client = VolumeHostsV2Client(
            self.auth_provider, **params)
        self.volume_quotas_client = VolumeQuotasClient(self.auth_provider,
                                                       **params)
        self.volume_quotas_v2_client = VolumeQuotasV2Client(self.auth_provider,
                                                            **params)
        self.volumes_extension_client = VolumeExtensionClient(
            self.auth_provider, **params)
        self.volumes_v2_extension_client = VolumeV2ExtensionClient(
            self.auth_provider, **params)
        self.volume_availability_zone_client = \
            VolumeAvailabilityZoneClient(self.auth_provider, **params)
        self.volume_v2_availability_zone_client = \
            VolumeV2AvailabilityZoneClient(self.auth_provider, **params)
        self.volume_types_v2_client = VolumeTypesV2Client(
            self.auth_provider, **params)

    def _set_object_storage_clients(self):
        params = {
            'service': CONF.object_storage.catalog_type,
            'region': CONF.object_storage.region or CONF.identity.region,
            'endpoint_type': CONF.object_storage.endpoint_type
        }
        params.update(self.default_params_with_timeout_values)

        # self.account_client = AccountClient(self.auth_provider, **params)
        # self.container_client = ContainerClient(self.auth_provider, **params)
        # self.object_client = ObjectClient(self.auth_provider, **params)


class AdminManager(Manager):

    """
    Manager object that uses the admin credentials for its
    managed client objects
    """

    def __init__(self, service=None):
        super(AdminManager, self).__init__(
            credentials=cred_provider.get_configured_credentials(
                'identity_admin'),
            service=service)