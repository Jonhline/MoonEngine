/* Mod-only SpecDB authoring. Included after addcar's pristine-table helpers. */
#include <math.h>
#include <limits.h>
#include "spectables.h"

enum {
    SPEC_CHASSIS_WEIGHT = 6, SPEC_CATALOG_WEIGHT = 12,
    SPEC_ENGINE_POWER = 0x1a, SPEC_ENGINE_TORQUE = 0x1c,
    SPEC_ENGINE_CURVE = 0x1e, SPEC_ENGINE_REDLINE = 0x51,
    SPEC_ENGINE_SHIFT = 0x50, SPEC_ENGINE_COUNT = 0x54, SPEC_ENGINE_RPMS = 0x55,
    SPEC_DRIVE_KIND = 0x10, SPEC_DRIVE_CENTER = 0x11,
    SPEC_GEAR_FINAL_MIN = 0x18, SPEC_GEAR_FINAL = 0x1a, SPEC_GEAR_FINAL_MAX = 0x1c,
    SPEC_GEAR_COUNT = 0x24, SPEC_GEAR_AUTO = 0x25
};
typedef struct { unsigned min, max; } mod_range;
typedef struct {
    const char *weight_arg, *torque_arg, *rpm_arg, *redline_arg, *drive_arg;
    const char *gears_arg, *final_arg, *ratios_arg;
    unsigned weight, torque_rpm, redline, gears, final, ratios[9], ratio_count;
    double torque;
    int drive;
    mod_range weights, torques, rpms, redlines, gear_counts, finals, gear_ratios, powers;
} mod_specs;
static unsigned spec_u16(const uint8_t *p) { return p[0] | (unsigned)p[1] << 8; }
static void spec_observe(mod_range *r, unsigned x)
{
    if (!x) return;
    if (!r->min || x < r->min) r->min = x;
    if (x > r->max) r->max = x;
}
static int spec_number(const char *name, const char *arg, double min, double max,
                       double quantum, double *out)
{
    char *end; double x = strtod(arg, &end);
    if (!*arg || *end || !isfinite(x) || x < min || x > max ||
        fabs(x / quantum - round(x / quantum)) > 1e-6) {
        fprintf(stderr, "%s must be %.6g..%.6g, in steps of %.6g (retail disc range)\n",
                name, min, max, quantum);
        return 0;
    }
    *out = x; return 1;
}
/* Original: func_00441B20 +0x0. record.c maps CHASSIS +6 into mass;
   004460C8/catalog_geometry separately reads +12. Measured Ka 965/965,
   M5 1715/1715, NSX 1340/1340 kg: update both, not merely the UI label.
   Limits below are measured from stock DEFAULT_PARTS, excluding empty rows. */
static int spec_measure(gt4_vol *v, const uint8_t *generic, size_t ng, mod_specs *s)
{
    gt4_spec_tables *tables = gt4_spec_tables_open(v, NULL);
    size_t nd; uint8_t *dp = read_table(v, "DEFAULT_PARTS.dbt", &nd);
    if (!tables || !dp) { free(dp); if (tables) gt4_spec_tables_close(tables); return 0; }
    uint8_t g[256], p[256], row[256];
    for (uint32_t i = 0; gt4_specdb_row_at(generic, ng, i, g, sizeof g) > 0; i++) {
        if (gt4_specdb_row(dp, nd, le32(g), p, sizeof p) != 208) continue;
        if (gt4_spec_tables_row(tables, 6, le32(p+0x28), 0, row, sizeof row) == 37)
            spec_observe(&s->weights, spec_u16(row+SPEC_CHASSIS_WEIGHT));
        if (gt4_spec_tables_row(tables, 12, le32(p+0x58), 0, row, sizeof row) == 113) {
            spec_observe(&s->redlines, row[SPEC_ENGINE_REDLINE]*100);
            spec_observe(&s->powers, spec_u16(row+SPEC_ENGINE_POWER));
            unsigned n = row[SPEC_ENGINE_COUNT];
            if (n > 24) continue;
            for (unsigned j=0; j<n; j++) {
                spec_observe(&s->torques, spec_u16(row+SPEC_ENGINE_CURVE+j*2));
                spec_observe(&s->rpms, row[SPEC_ENGINE_RPMS+j]*100);
            }
        }
        if (gt4_spec_tables_row(tables, 11, le32(p+0x50), 0, row, sizeof row) == 45) {
            spec_observe(&s->gear_counts, row[SPEC_GEAR_COUNT]);
            spec_observe(&s->finals, spec_u16(row+SPEC_GEAR_FINAL));
            for (unsigned j=0; j<row[SPEC_GEAR_COUNT] && j<9; j++)
                spec_observe(&s->gear_ratios, spec_u16(row+j*2));
        }
    }
    free(dp); gt4_spec_tables_close(tables);
    printf("retail limits: mass %u..%u kg; curve %u..%u; RPM %u..%u; redline %u..%u; PS %u..%u; gears %u..%u; final %u..%u; ratios %u..%u\n",
           s->weights.min,s->weights.max,s->torques.min,s->torques.max,s->rpms.min,s->rpms.max,
           s->redlines.min,s->redlines.max,s->powers.min,s->powers.max,s->gear_counts.min,s->gear_counts.max,
           s->finals.min,s->finals.max,s->gear_ratios.min,s->gear_ratios.max);
    double x;
#define PARSE(arg, field, lo, hi, step, factor) \
    if (s->arg) { if (!spec_number("--" #field, s->arg,lo,hi,step,&x)) return 0; s->field=(unsigned)round(x*factor); }
    PARSE(weight_arg,weight,s->weights.min,s->weights.max,1,1)
    PARSE(rpm_arg,torque_rpm,s->rpms.min,s->rpms.max,100,1)
    PARSE(redline_arg,redline,s->redlines.min,s->redlines.max,100,1)
    PARSE(gears_arg,gears,s->gear_counts.min,s->gear_counts.max,1,1)
    PARSE(final_arg,final,s->finals.min/1000.0,s->finals.max/1000.0,0.001,1000)
#undef PARSE
    if (s->torque_arg && !spec_number("--torque",s->torque_arg,s->torques.min*0.098,s->torques.max*0.098,0.001,&s->torque)) return 0;
    s->drive=-1;
    if (s->drive_arg) {
        const char *names[]={"FR","FF","4WD","MR","RR"};
        for (int i=0;i<5;i++) if (!strcmp(s->drive_arg,names[i])) s->drive=i;
        if (s->drive<0) { fprintf(stderr,"--drivetrain must be FR|FF|MR|RR|4WD\n"); return 0; }
    }
    if (s->ratios_arg) {
        const char *p=s->ratios_arg;
        while (*p) {
            char *end; double r=strtod(p,&end);
            if (end==p || (*end && *end!=',') || !isfinite(r) ||
                s->ratio_count==9 || r*1000<s->gear_ratios.min || r*1000>s->gear_ratios.max ||
                fabs(r*1000-round(r*1000))>1e-6) {
                fprintf(stderr,"--ratios needs up to nine comma-separated ratios %.3f..%.3f, step .001\n",s->gear_ratios.min/1000.,s->gear_ratios.max/1000.); return 0;
            }
            s->ratios[s->ratio_count++]=(unsigned)round(r*1000);
            if (*end && !end[1]) { fprintf(stderr,"--ratios has a trailing comma\n");return 0; }
            p=*end?end+1:end;
        }
        if (!s->ratio_count || (s->gears_arg && s->gears!=s->ratio_count)) {
            fprintf(stderr,"--ratios count must equal --gears\n");return 0;
        }
        s->gears=s->ratio_count;
        if (s->gears<s->gear_counts.min || s->gears>s->gear_counts.max) return 0;
        for (unsigned i=1;i<s->ratio_count;i++) if (s->ratios[i]>=s->ratios[i-1]) {
            fprintf(stderr,"--ratios must decrease strictly from first to last gear\n");return 0;
        }
    }
    return 1;
}
/* Original: func_00441C58 +0x0 -> 00361908 (engine_params), 0034F3F8.
   Ka: +54=13, +55..=10,15,...70 (hundreds RPM), +1e..=844..1070,
   +51=70 (7000 RPM). setup.c converts each torque unit to 0.098 N m;
   the displayed PS is sampled from that curve, not ENGINE +1a's label.
   A two-segment authored curve is deliberately generated when torque AND
   power are specified: solve its tail against the same 100-RPM peak scan.
   It is mod content, not a reconstruction of a real engine's torque curve. */
static void spec_curve_peaks(const uint8_t *r, double *ps, double *nm, unsigned *peak_rpm, unsigned *power_rpm)
{
    unsigned n=r[SPEC_ENGINE_COUNT], end=r[SPEC_ENGINE_REDLINE]*100;
    *ps=*nm=0; *peak_rpm=0;
    if (!n || n>24) return;
    for (unsigned rpm=r[SPEC_ENGINE_RPMS]*100;rpm<=end;rpm+=100) {
        unsigned k=0;
        while (k+1<n && rpm>r[SPEC_ENGINE_RPMS+k+1]*100u) k++;
        double t=spec_u16(r+SPEC_ENGINE_CURVE+k*2);
        if (k+1<n) {
            unsigned a=r[SPEC_ENGINE_RPMS+k]*100,b=r[SPEC_ENGINE_RPMS+k+1]*100;
            if (b>a) t+=(spec_u16(r+SPEC_ENGINE_CURVE+(k+1)*2)-t)*(rpm-(double)a)/(b-a);
        }
        t*=0.098; double p=t/9.8*rpm/716.2;
        if (t>*nm) { *nm=t; *peak_rpm=rpm; }
        if (p>*ps) { *ps=p; if (power_rpm) *power_rpm=rpm; }
    }
}
static int spec_engine(uint8_t *row, const mod_specs *s, const char *power_arg)
{
    double old_ps,old_t,desired_ps=0;unsigned peak;
    spec_curve_peaks(row,&old_ps,&old_t,&peak,NULL);
    if (!old_ps || !old_t) { fprintf(stderr,"ENGINE curve has no usable stock points\n");return 0; }
    if (power_arg && !spec_number("--power",power_arg,s->powers.min,s->powers.max,1,&desired_ps)) return 0;
    unsigned n=row[SPEC_ENGINE_COUNT], red=s->redline?s->redline:row[SPEC_ENGINE_REDLINE]*100;
    unsigned tr=s->torque_rpm?s->torque_rpm:peak;
    if (tr>red || red<row[0x52]*10u+200) { fprintf(stderr,"torque RPM must not exceed redline, and redline must exceed idle\n"); return 0; }
    if (s->torque_arg && power_arg) {
        double target=s->torque;
        if (tr>=red || desired_ps<target/9.8*tr/716.2 || desired_ps>target/9.8*red/716.2) {
            fprintf(stderr,"power/torque/RPM conflict: need torque*peakRPM/7018.76 <= PS <= torque*redline/7018.76, with peakRPM < redline\n");return 0;
        }
        unsigned start=row[SPEC_ENGINE_RPMS]*100;
        if (start>=tr) start=tr-100;
        if (start<s->rpms.min) { fprintf(stderr,"torque peak needs a preceding in-range RPM point\n");return 0; }
        memset(row+SPEC_ENGINE_CURVE,0,48);memset(row+SPEC_ENGINE_RPMS,0,24);
        row[SPEC_ENGINE_COUNT]=3;row[SPEC_ENGINE_RPMS]=(uint8_t)(start/100);
        row[SPEC_ENGINE_RPMS+1]=(uint8_t)(tr/100);row[SPEC_ENGINE_RPMS+2]=(uint8_t)(red/100);
        row[SPEC_ENGINE_REDLINE]=(uint8_t)(red/100);
        put16(row+SPEC_ENGINE_CURVE,(unsigned)round(target/0.098*start/tr));
        put16(row+SPEC_ENGINE_CURVE+2,(unsigned)round(target/0.098));
        unsigned lo=s->torques.min,hi=(unsigned)round(target/0.098),best=lo;double error=HUGE_VAL;
        while (lo<=hi) {
            unsigned mid=lo+(hi-lo)/2;double p,t;unsigned at;
            put16(row+SPEC_ENGINE_CURVE+4,mid);spec_curve_peaks(row,&p,&t,&at,NULL);
            if (fabs(p-desired_ps)<error) {error=fabs(p-desired_ps);best=mid;}
            if (p<desired_ps) lo=mid+1; else { if (!mid) break;hi=mid-1; }
        }
        put16(row+SPEC_ENGINE_CURVE+4,best);
    } else {
        /* Warp the donor RPM knots about its torque peak, retaining order. */
        unsigned prev=0;
        for (unsigned i=0;i<n;i++) {
            unsigned rpm=row[SPEC_ENGINE_RPMS+i]*100;
            double mapped=rpm;
            if (s->rpm_arg && row[SPEC_ENGINE_REDLINE]*100u<=peak) { fprintf(stderr,"donor curve has no span above peak RPM\n");return 0; }
            if (s->rpm_arg) mapped=rpm<=peak ? rpm*(double)tr/peak : tr+(rpm-peak)*(double)(red-tr)/(row[SPEC_ENGINE_REDLINE]*100-peak);
            unsigned rounded=(unsigned)round(mapped/100)*100;
            if (rounded<s->rpms.min || rounded>s->rpms.max || (i && rounded<=prev)) {
                fprintf(stderr,"requested RPM warp collapses/outgrows donor knots; choose another torque RPM\n");return 0;
            }
            row[SPEC_ENGINE_RPMS+i]=(uint8_t)(rounded/100);prev=rounded;
        }
        row[SPEC_ENGINE_REDLINE]=(uint8_t)(red/100);
        double p,t;unsigned at;spec_curve_peaks(row,&p,&t,&at,NULL);
        double factor=s->torque_arg?s->torque/t:power_arg?desired_ps/p:1;
        for (unsigned i=0;i<n;i++) {
            unsigned value=(unsigned)round(spec_u16(row+SPEC_ENGINE_CURVE+i*2)*factor);
            if (value<s->torques.min || value>s->torques.max) {fprintf(stderr,"requested curve exceeds retail torque range\n");return 0;}
            put16(row+SPEC_ENGINE_CURVE+i*2,value);
        }
    }
    if (row[SPEC_ENGINE_SHIFT]>=red/100) row[SPEC_ENGINE_SHIFT]=(uint8_t)(red/100-2);
    double p,t;unsigned at;spec_curve_peaks(row,&p,&t,&at,NULL);
    for (unsigned i=0;i<row[SPEC_ENGINE_COUNT];i++) {
        unsigned value=spec_u16(row+SPEC_ENGINE_CURVE+i*2);
        if (value<s->torques.min || value>s->torques.max) {
            fprintf(stderr,"authored curve point exceeds retail torque range\n");return 0;
        }
    }
    if (round(p)<s->powers.min || round(p)>s->powers.max) {
        fprintf(stderr,"authored power %.3f PS exceeds retail range %u..%u\n",p,s->powers.min,s->powers.max);return 0;
    }
    if ((power_arg && fabs(p-desired_ps)>0.5) || (s->torque_arg && fabs(t-s->torque)>0.099) || (s->rpm_arg && at!=tr)) {
        fprintf(stderr,"requested peaks are not representable by this curve (%.3f PS, %.3f Nm @ %u)\n",p,t,at);return 0;
    }
    put16(row+SPEC_ENGINE_POWER,(unsigned)round(p));put16(row+SPEC_ENGINE_TORQUE,(unsigned)round(t/0.098));
    printf("authored ENGINE: %.3f PS, %.3f Nm @ %u RPM, redline %u\n",p,t,at,red);
    return 1;
}
/* Original: func_00441E28 +0x0 / func_0034BC98 +0x0.
   Disc +10: M5=0 FR, Ka=1 FF, Evo VI=2 4WD, NSX=3 MR, BTR=4 RR.
   4WD needs centre fields too: Evo VI +11..14 = 02 0a 32 1e;
   Ka's FE sentinel would not describe a 4WD centre. Authoring policy:
   when changing to 4WD, inherit these centre fields from that disc car,
   while retaining the chosen physics donor's inertia/braking parameters. */
static int spec_drive(uint8_t *row, const mod_specs *s, gt4_vol *v)
{
    if (s->drive==2) {
        gt4_spec_tables *tables=gt4_spec_tables_open(v,NULL);
        size_t ni,ng,nd;uint8_t *idi=read_table(v,"GENERIC_CAR.idi",&ni);
        uint8_t *generic=read_table(v,"GENERIC_CAR.dbt",&ng);
        uint8_t *parts=read_table(v,"DEFAULT_PARTS.dbt",&nd);
        uint8_t g[256],p[256],sample[256];uint32_t id;
        int ok=tables && idi && generic && parts &&
            gt4_specdb_label(idi,ni,"evo6_wrc_99",&id) &&
            gt4_specdb_row(generic,ng,id,g,sizeof g)==24 &&
            gt4_specdb_row(parts,nd,le32(g),p,sizeof p)==208 &&
            gt4_spec_tables_row(tables,10,le32(p+0x48),0,sample,sizeof sample)==21;
        if (ok) memcpy(row+SPEC_DRIVE_CENTER,sample+SPEC_DRIVE_CENTER,4);
        free(idi);free(generic);free(parts);
        if (tables) gt4_spec_tables_close(tables);
        if (!ok) { fprintf(stderr,"4WD centre reference evo6_wrc_99 is unavailable\n");return 0; }
    }
    row[SPEC_DRIVE_KIND]=(uint8_t)s->drive;
    return 1;
}
/* Original: func_00444A88 +0x0 / func_00441E78 +0x0.
   Ka GEAR +0..8 = 3333,1955,1286,926,733; +1a=4056; +24=5.
   M5 has 7 ratios, final 3620. Both use thousandths. +25 generates auto
   ratios; explicit ratios/count must disable that generation, not be lost. */
static int spec_gear(uint8_t *row, const mod_specs *s)
{
    unsigned count=s->gears?s->gears:row[SPEC_GEAR_COUNT];
    if (s->gears || s->ratios_arg) {
        for (unsigned i=0;i<count;i++) {
            unsigned r=s->ratios_arg?s->ratios[i]:spec_u16(row+i*2);
            if (r<s->gear_ratios.min || r>s->gear_ratios.max) {
                fprintf(stderr,"--gears needs --ratios for every added/missing gear\n");return 0;
            }
            put16(row+i*2,r);
        }
        row[SPEC_GEAR_COUNT]=(uint8_t)count;row[SPEC_GEAR_AUTO]=0;
        for (unsigned i=count;i<9;i++) put16(row+i*2,0);
    }
    if (s->final_arg) for (unsigned at=SPEC_GEAR_FINAL_MIN;at<=SPEC_GEAR_FINAL_MAX;at+=2) put16(row+at,s->final);
    printf("authored GEAR: %u forward gears; final %.3f\n",count,spec_u16(row+SPEC_GEAR_FINAL)/1000.);
    return 1;
}
